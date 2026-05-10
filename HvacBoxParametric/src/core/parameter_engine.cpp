/**
 * @file parameter_engine.cpp
 * @brief 参数引擎核心实现
 * @version 1.0
 * @date 2026-05-10
 */

#include "core/parameter_engine.h"

/* ============================================================
 * 生命周期管理
 * ============================================================ */

bool HvacParameterEngine::initialize(void* pSession)
{
    if (m_initialized) return true;

    // 初始化UF环境 (全版本兼容)
    int rc = UF_initialize();
    if (rc != 0) return false;

#if HVAC_USE_NXOPEN_CPP
    // NX1980+: 获取Session和WorkPart
    NXOpen::Session* session = NXOpen::Session::GetSession();
    m_buildCtx.pSession = session;
    m_buildCtx.pWorkPart = session->Parts()->Work();
    if (m_buildCtx.pWorkPart != nullptr) {
        m_buildCtx.workPartTag = m_buildCtx.pWorkPart->Tag();
    }
#else
    // NX12: 通过UF获取WorkPart tag
    UF_PART_ask_display_part(&m_buildCtx.workPartTag);
#endif

    // 注册依赖关系
    registerDependencies();
    topologicalSort();

    m_initialized = true;
    return true;
}

void HvacParameterEngine::shutdown()
{
    if (!m_initialized) return;

    m_callbacks.clear();
    m_dependencyGraph.clear();
    m_sortedBuildOrder.clear();

    UF_terminate();
    m_initialized = false;
}

/* ============================================================
 * 参数操作
 * ============================================================ */

ParamStatus HvacParameterEngine::setParameter(
    const std::string& paramId, double newValue)
{
    // 1. 范围校验
    ParamStatus status = m_params.validateSingleParam(paramId, newValue);
    if (status == ParamStatus::FAIL_OUT_RANGE) {
        return status; // 拒绝赋值
    }

    // 2. 应用值并保存旧值
    double oldValue = 0.0;
    if (!applyValueToParam(paramId, newValue, oldValue)) {
        return ParamStatus::FAIL_CONFLICT; // 未知参数ID
    }

    // 3. 触发Level-2重算
    m_params.recalculateLevel2();

    // 4. 构造事件并通知回调
    ParamChangeEvent event;
    event.paramId = paramId;
    event.oldValue = oldValue;
    event.newValue = newValue;
    event.status = status;
    notifyCallbacks(event);

    return status;
}

std::vector<ParamChangeEvent> HvacParameterEngine::setParametersBatch(
    const std::map<std::string, double>& changes)
{
    std::vector<ParamChangeEvent> failures;

    // 先校验全部参数
    for (const auto& kv : changes) {
        ParamStatus st = m_params.validateSingleParam(kv.first, kv.second);
        if (st == ParamStatus::FAIL_OUT_RANGE) {
            ParamChangeEvent ev;
            ev.paramId = kv.first;
            ev.newValue = kv.second;
            ev.status = st;
            failures.push_back(ev);
        }
    }

    // 有任何FAIL则整批拒绝
    if (!failures.empty()) return failures;

    // 批量应用
    for (const auto& kv : changes) {
        double oldVal = 0.0;
        applyValueToParam(kv.first, kv.second, oldVal);
    }

    // 只做一次Level-2重算 (避免多次重复计算)
    m_params.recalculateLevel2();

    return failures; // 空=全部成功
}

bool HvacParameterEngine::loadParameters(const std::string& jsonPath)
{
    return m_params.loadFromJson(jsonPath);
}

bool HvacParameterEngine::saveParameters(const std::string& jsonPath) const
{
    return m_params.saveToJson(jsonPath);
}

/* ============================================================
 * 构建调度
 * ============================================================ */

BuildStatus HvacParameterEngine::executeBuild()
{
    if (!m_initialized) return BuildStatus::FAILED;

    m_buildCtx.status = BuildStatus::IN_PROGRESS;
    m_buildCtx.logMessages.clear();

    // 1. 创建Undo标记
    int undoMark = createUndoMark("HVAC_FullBuild");

    try {
        // 2. 导出参数到NX Expression
        m_params.exportToNxExpressions(m_buildCtx.workPartTag);
        m_buildCtx.logMessages.push_back("Parameters exported to NX Expressions.");

        // 3. Module-1: 壳体构建
        // (由外部调用ShellBuilder, 此处仅作调度框架)
        m_buildCtx.logMessages.push_back("Module-1 Shell build dispatched.");

        // 4. 更新模型
        UF_MODL_update();

        // 5. 删除Undo标记(成功后不需要回滚)
        deleteUndoMark(undoMark);

        m_buildCtx.status = BuildStatus::SUCCESS;
        m_buildCtx.logMessages.push_back("Build completed successfully.");

    } catch (const std::exception& ex) {
        // 构建失败 → 回滚
        m_buildCtx.logMessages.push_back(
            std::string("Build FAILED: ") + ex.what());
        rollbackToMark(undoMark);
        m_buildCtx.status = BuildStatus::ROLLED_BACK;
    }

    return m_buildCtx.status;
}

BuildStatus HvacParameterEngine::executeIncrementalBuild(
    const std::vector<std::string>& changedParams)
{
    // Phase-1简化: 增量构建当前等同于全量构建
    // Phase-3实现完整的增量判断逻辑
    return executeBuild();
}

/* ============================================================
 * 校验
 * ============================================================ */

std::vector<ValidationItem> HvacParameterEngine::runValidation()
{
    std::vector<ValidationItem> results;

    // L3-001: 风门摆角范围校验(简化版, Phase-3接入运动仿真)
    {
        ValidationItem item;
        item.id = "L3-001";
        item.description = "Temp door sweep angle in valid range";
        item.actualValue = m_params.level2().tempDoorSweepAngle;
        item.limitValue = HvacConst::TEMP_DOOR_SWEEP_MAX;

        if (item.actualValue >= HvacConst::TEMP_DOOR_SWEEP_MIN &&
            item.actualValue <= HvacConst::TEMP_DOOR_SWEEP_MAX) {
            item.result = ValidationResult::PASS;
            item.message = "Sweep angle OK: " + std::to_string(item.actualValue) + " deg";
        } else {
            item.result = ValidationResult::FAIL;
            item.message = "Sweep angle OUT OF RANGE: " + std::to_string(item.actualValue);
        }
        results.push_back(item);
    }

    // L3-002: 壁厚校验
    {
        ValidationItem item;
        item.id = "L3-002";
        item.description = "Wall thickness within DFM limits";
        item.actualValue = m_params.level1().wallThickness;
        item.limitValue = HvacConst::MAX_WALL_THICKNESS;

        if (item.actualValue >= HvacConst::MIN_WALL_THICKNESS &&
            item.actualValue <= HvacConst::MAX_WALL_THICKNESS) {
            item.result = ValidationResult::PASS;
        } else {
            item.result = ValidationResult::FAIL;
        }
        item.message = "Wall thickness: " + std::to_string(item.actualValue) + " mm";
        results.push_back(item);
    }

    // L3-007: 执行器力矩校验(简化)
    {
        ValidationItem item;
        item.id = "L3-007";
        item.description = "Actuator torque safety factor";
        // 简化: 连杆长度过短会导致力矩过大
        item.actualValue = m_params.level2().couplerLength;
        item.limitValue = 10.0; // 最小连杆长度

        if (item.actualValue > 15.0) {
            item.result = ValidationResult::PASS;
            item.message = "Coupler length OK: " + std::to_string(item.actualValue) + " mm";
        } else if (item.actualValue > 10.0) {
            item.result = ValidationResult::WARN;
            item.message = "Coupler length near limit: " + std::to_string(item.actualValue);
        } else {
            item.result = ValidationResult::FAIL;
            item.message = "Coupler too short, high torque risk";
        }
        results.push_back(item);
    }

    // L3-008: 冷凝水排放坡度
    {
        ValidationItem item;
        item.id = "L3-008";
        item.description = "Condensate drain slope >= 3 deg";
        item.actualValue = m_params.level1().evapTiltAngle;
        item.limitValue = 3.0;

        if (item.actualValue >= 3.0) {
            item.result = ValidationResult::PASS;
        } else if (item.actualValue >= 1.0) {
            item.result = ValidationResult::WARN;
            item.message = "Slope marginal, condensate drainage may be slow";
        } else {
            item.result = ValidationResult::FAIL;
            item.message = "Insufficient slope for condensate drainage";
        }
        item.message += " (" + std::to_string(item.actualValue) + " deg)";
        results.push_back(item);
    }

    return results;
}

/* ============================================================
 * Undo/Redo管理
 * ============================================================ */

int HvacParameterEngine::createUndoMark(const std::string& markName)
{
#if HVAC_USE_NXOPEN_CPP
    if (m_buildCtx.pSession != nullptr) {
        NXOpen::Session::UndoMarkId markId =
            m_buildCtx.pSession->SetUndoMark(
                NXOpen::Session::MarkVisibilityVisible, markName);
        m_currentUndoMark = static_cast<int>(markId);
        m_undoMarks.push_back(m_currentUndoMark);
        return m_currentUndoMark;
    }
#endif
    // NX12 fallback: UF Undo (功能有限)
    m_currentUndoMark = static_cast<int>(m_undoMarks.size());
    m_undoMarks.push_back(m_currentUndoMark);
    return m_currentUndoMark;
}

bool HvacParameterEngine::rollbackToMark(int markId)
{
#if HVAC_USE_NXOPEN_CPP
    if (m_buildCtx.pSession != nullptr) {
        try {
            m_buildCtx.pSession->UndoToMark(
                static_cast<NXOpen::Session::UndoMarkId>(markId), nullptr);
            return true;
        } catch (const NXOpen::NXException&) {
            return false;
        }
    }
#endif
    // NX12: 简化回滚(日志记录)
    return false;
}

void HvacParameterEngine::deleteUndoMark(int markId)
{
#if HVAC_USE_NXOPEN_CPP
    if (m_buildCtx.pSession != nullptr) {
        try {
            m_buildCtx.pSession->DeleteUndoMark(
                static_cast<NXOpen::Session::UndoMarkId>(markId), nullptr);
        } catch (...) {}
    }
#endif
    // 从列表中移除
    m_undoMarks.erase(
        std::remove(m_undoMarks.begin(), m_undoMarks.end(), markId),
        m_undoMarks.end());
}

/* ============================================================
 * 回调管理
 * ============================================================ */

int HvacParameterEngine::registerChangeCallback(ParamChangeCallback callback)
{
    int id = m_nextCallbackId++;
    m_callbacks[id] = std::move(callback);
    return id;
}

void HvacParameterEngine::unregisterChangeCallback(int callbackId)
{
    m_callbacks.erase(callbackId);
}

void HvacParameterEngine::notifyCallbacks(const ParamChangeEvent& event)
{
    for (const auto& kv : m_callbacks) {
        if (kv.second) {
            kv.second(event);
        }
    }
}

/* ============================================================
 * 依赖图管理
 * ============================================================ */

std::vector<std::string> HvacParameterEngine::getAffectedModules(
    const std::string& paramId) const
{
    std::vector<std::string> affected;

    // Phase-1: 所有L1参数变更都影响Module-1(壳体)
    // Phase-3: 基于依赖图精确判断
    affected.push_back("Module_1_Shell");

    // 风门相关参数额外影响Module-5
    if (paramId.find("tempDoor") != std::string::npos ||
        paramId.find("modeDoor") != std::string::npos ||
        paramId.find("actuator") != std::string::npos ||
        paramId.find("doorAxis") != std::string::npos) {
        affected.push_back("Module_5_Motion");
    }

    return affected;
}

void HvacParameterEngine::registerDependencies()
{
    // 模块依赖关系注册
    // Module-1 (壳体): 依赖所有Level-1尺寸参数
    DependencyNode shellNode;
    shellNode.nodeId = "Module_1_Shell";
    shellNode.dependsOn = {}; // 直接依赖Level-1参数(无模块依赖)
    m_dependencyGraph["Module_1_Shell"] = shellNode;

    // Module-2 (风道): 依赖Module-1
    DependencyNode ductNode;
    ductNode.nodeId = "Module_2_Duct";
    ductNode.dependsOn = {"Module_1_Shell"};
    m_dependencyGraph["Module_2_Duct"] = ductNode;

    // Module-3 (接口): 依赖Module-1
    DependencyNode ifNode;
    ifNode.nodeId = "Module_3_Interface";
    ifNode.dependsOn = {"Module_1_Shell"};
    m_dependencyGraph["Module_3_Interface"] = ifNode;

    // Module-5 (运动): 依赖Module-2, Module-3
    DependencyNode motionNode;
    motionNode.nodeId = "Module_5_Motion";
    motionNode.dependsOn = {"Module_2_Duct", "Module_3_Interface"};
    m_dependencyGraph["Module_5_Motion"] = motionNode;

    // Module-4 (密封): 依赖Module-1, Module-5
    DependencyNode sealNode;
    sealNode.nodeId = "Module_4_Seal";
    sealNode.dependsOn = {"Module_1_Shell", "Module_5_Motion"};
    m_dependencyGraph["Module_4_Seal"] = sealNode;

    // Module-6 (校验): 依赖所有
    DependencyNode valNode;
    valNode.nodeId = "Module_6_Validate";
    valNode.dependsOn = {"Module_1_Shell", "Module_2_Duct",
                         "Module_3_Interface", "Module_4_Seal", "Module_5_Motion"};
    m_dependencyGraph["Module_6_Validate"] = valNode;

    // Module-7 (出图): 依赖Module-6
    DependencyNode drawNode;
    drawNode.nodeId = "Module_7_Drawing";
    drawNode.dependsOn = {"Module_6_Validate"};
    m_dependencyGraph["Module_7_Drawing"] = drawNode;

    // 建立反向依赖(dependents)
    for (auto& kv : m_dependencyGraph) {
        for (const auto& dep : kv.second.dependsOn) {
            if (m_dependencyGraph.count(dep)) {
                m_dependencyGraph[dep].dependents.push_back(kv.first);
            }
        }
    }
}

void HvacParameterEngine::topologicalSort()
{
    m_sortedBuildOrder.clear();

    // Kahn算法拓扑排序
    std::map<std::string, int> inDegree;
    for (const auto& kv : m_dependencyGraph) {
        if (inDegree.find(kv.first) == inDegree.end()) {
            inDegree[kv.first] = 0;
        }
        for (const auto& dep : kv.second.dependsOn) {
            inDegree[kv.first]++;
        }
    }

    // 入度为0的节点入队
    std::vector<std::string> queue;
    for (const auto& kv : inDegree) {
        if (kv.second == 0) queue.push_back(kv.first);
    }

    int order = 0;
    while (!queue.empty()) {
        std::string current = queue.back();
        queue.pop_back();

        m_sortedBuildOrder.push_back(current);
        m_dependencyGraph[current].buildOrder = order++;

        // 减少下游节点入度
        for (const auto& dep : m_dependencyGraph[current].dependents) {
            inDegree[dep]--;
            if (inDegree[dep] == 0) {
                queue.push_back(dep);
            }
        }
    }
}

/* ============================================================
 * 内部: 参数值映射与赋值
 * ============================================================ */

bool HvacParameterEngine::applyValueToParam(
    const std::string& paramId, double value, double& oldValue)
{
    auto& L1 = m_params.level1();

    if (paramId == "L1_001_boxLengthX") {
        oldValue = L1.boxLengthX; L1.boxLengthX = value;
    } else if (paramId == "L1_002_boxWidthY") {
        oldValue = L1.boxWidthY; L1.boxWidthY = value;
    } else if (paramId == "L1_003_boxHeightZ") {
        oldValue = L1.boxHeightZ; L1.boxHeightZ = value;
    } else if (paramId == "L1_004_evapTiltAngle") {
        oldValue = L1.evapTiltAngle; L1.evapTiltAngle = value;
    } else if (paramId == "L1_005_heaterTiltAngle") {
        oldValue = L1.heaterTiltAngle; L1.heaterTiltAngle = value;
    } else if (paramId == "L1_006_coreSpacing") {
        oldValue = L1.coreSpacing; L1.coreSpacing = value;
    } else if (paramId == "L1_007_partingPlaneZ") {
        oldValue = L1.partingPlaneZ; L1.partingPlaneZ = value;
    } else if (paramId == "L1_008_wallThickness") {
        oldValue = L1.wallThickness; L1.wallThickness = value;
    } else if (paramId == "L1_009_sealGrooveWidth") {
        oldValue = L1.sealGrooveWidth; L1.sealGrooveWidth = value;
    } else if (paramId == "L1_010_doorAxisToWall") {
        oldValue = L1.doorAxisToWall; L1.doorAxisToWall = value;
    } else if (paramId == "L1_011_defOutletZ") {
        oldValue = L1.defOutletZ; L1.defOutletZ = value;
    } else if (paramId == "L1_012_faceOutletZ") {
        oldValue = L1.faceOutletZ; L1.faceOutletZ = value;
    } else if (paramId == "L1_013_footOutletZ") {
        oldValue = L1.footOutletZ; L1.footOutletZ = value;
    } else if (paramId == "L1_014_tempDoorCenterX") {
        oldValue = L1.tempDoorCenterX; L1.tempDoorCenterX = value;
    } else if (paramId == "L1_015_tempDoorCenterZ") {
        oldValue = L1.tempDoorCenterZ; L1.tempDoorCenterZ = value;
    } else if (paramId == "L1_016_modeDoorCenterX") {
        oldValue = L1.modeDoorCenterX; L1.modeDoorCenterX = value;
    } else if (paramId == "L1_017_actuatorOffsetY") {
        oldValue = L1.actuatorOffsetY; L1.actuatorOffsetY = value;
    } else {
        return false; // 未知参数ID
    }

    return true;
}
