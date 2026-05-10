/**
 * @file parameter_engine.h
 * @brief HVAC箱体参数化系统 - 参数引擎核心
 * @details 负责参数依赖关系管理、联动触发、变更回滚、模块构建调度。
 *          作为整个系统的"大脑"，协调参数变更→Level-2重算→几何重建→校验的完整流水线。
 *
 * 【核心职责】
 * 1. 管理参数依赖有向图(DAG)，拓扑排序确定重建顺序
 * 2. 参数变更前后的Undo/Redo管理
 * 3. 调度各Module按依赖顺序执行构建
 * 4. 变更失败时的自动回滚
 * 5. 提供参数变更通知回调机制
 *
 * @version 1.0
 * @date 2026-05-10
 */

#ifndef HVAC_PARAMETER_ENGINE_H
#define HVAC_PARAMETER_ENGINE_H

#include "hvac_parameters.h"

/* 前向声明 */
class HvacShellBuilder;
class HvacValidationEngine;

/* ============================================================
 * 参数变更事件
 * ============================================================ */

/**
 * @struct ParamChangeEvent
 * @brief 参数变更事件描述
 */
struct ParamChangeEvent {
    std::string paramId;        ///< 变更参数ID (如 "L1_001_boxLengthX")
    double oldValue = 0.0;      ///< 变更前的值
    double newValue = 0.0;      ///< 变更后的值
    ParamStatus status = ParamStatus::VALID; ///< 变更校验结果
};

/**
 * @typedef ParamChangeCallback
 * @brief 参数变更通知回调函数类型
 */
using ParamChangeCallback = std::function<void(const ParamChangeEvent&)>;

/* ============================================================
 * 依赖关系节点
 * ============================================================ */

/**
 * @struct DependencyNode
 * @brief 依赖图中的节点
 */
struct DependencyNode {
    std::string nodeId;                     ///< 节点ID (参数ID或模块名)
    std::vector<std::string> dependsOn;     ///< 本节点依赖的上游节点ID列表
    std::vector<std::string> dependents;    ///< 依赖本节点的下游节点ID列表
    int buildOrder = -1;                    ///< 拓扑排序后的构建顺序 (-1=未排序)
    bool needsRebuild = false;              ///< 是否需要重建
};

/* ============================================================
 * 参数引擎主类
 * ============================================================ */

/**
 * @class HvacParameterEngine
 * @brief 参数引擎 - 系统核心调度器
 * @details 单例模式，管理参数全生命周期。
 *
 * 典型使用流程:
 * 1. initialize() - 初始化引擎
 * 2. loadParameters() - 加载参数配置
 * 3. setParameter() - 用户修改参数(自动触发联动)
 * 4. executeBuild() - 执行几何构建
 * 5. runValidation() - 运行合规性校验
 */
class HvacParameterEngine {
public:
    /** 获取单例实例 */
    static HvacParameterEngine& instance() {
        static HvacParameterEngine inst;
        return inst;
    }

    /* ---- 生命周期管理 ---- */

    /**
     * @brief 初始化参数引擎
     * @param pSession NXOpen会话指针 (NX1980+可用, NX12传nullptr)
     * @return true=成功
     * @details 初始化NX环境、注册依赖关系、设置Undo机制
     *
     * 【NX API兼容性】
     * - NX12: 使用UF_initialize()
     * - NX1980+: 使用NXOpen::Session::GetSession()
     * 所有版本均支持。
     */
    bool initialize(void* pSession = nullptr);

    /**
     * @brief 关闭引擎，释放资源
     */
    void shutdown();

    /**
     * @brief 引擎是否已初始化
     */
    bool isInitialized() const { return m_initialized; }

    /* ---- 参数操作 ---- */

    /**
     * @brief 获取参数集合(只读)
     */
    const HvacParameterSet& params() const { return m_params; }

    /**
     * @brief 获取参数集合(可写, 直接修改不触发联动)
     * @warning 直接修改后需手动调用 notifyParameterChanged()
     */
    HvacParameterSet& paramsMutable() { return m_params; }

    /**
     * @brief 安全设置单个Level-1参数(带校验和联动)
     * @param paramId 参数标识 (如 "L1_001_boxLengthX")
     * @param newValue 新值
     * @return 参数状态 (VALID=设置成功并触发联动, FAIL=拒绝)
     *
     * 【处理流程】
     * 1. 范围校验 → FAIL则拒绝赋值
     * 2. 保存旧值(用于回滚)
     * 3. 赋值
     * 4. 触发Level-2重算
     * 5. 通知注册的回调
     */
    ParamStatus setParameter(const std::string& paramId, double newValue);

    /**
     * @brief 批量设置参数(减少多次联动开销)
     * @param changes 参数ID-值映射
     * @return 失败的参数列表
     */
    std::vector<ParamChangeEvent> setParametersBatch(
        const std::map<std::string, double>& changes);

    /**
     * @brief 从JSON文件加载参数
     */
    bool loadParameters(const std::string& jsonPath);

    /**
     * @brief 保存当前参数到JSON
     */
    bool saveParameters(const std::string& jsonPath) const;

    /* ---- 构建调度 ---- */

    /**
     * @brief 执行完整构建流水线
     * @return 构建状态
     *
     * 【执行顺序】(基于依赖图拓扑排序)
     * 1. 创建UndoMark
     * 2. 导出参数到NX Expression
     * 3. Module-1 壳体构建
     * 4. (Phase-2) Module-2 风道 / Module-3 接口
     * 5. (Phase-3) Module-5 运动机构
     * 6. (Phase-4) Module-4 密封 / Module-6 校验
     * 7. 构建失败 → 回滚到UndoMark
     */
    BuildStatus executeBuild();

    /**
     * @brief 仅重建受影响的模块(增量构建)
     * @param changedParams 变更的参数ID列表
     * @return 构建状态
     */
    BuildStatus executeIncrementalBuild(const std::vector<std::string>& changedParams);

    /* ---- 校验 ---- */

    /**
     * @brief 运行Level-3全量校验
     * @return 校验结果列表
     */
    std::vector<ValidationItem> runValidation();

    /* ---- Undo/Redo管理 ---- */

    /**
     * @brief 创建Undo标记(构建前调用)
     * @param markName 标记名称
     * @return 标记ID
     *
     * 【NX API】
     * - NX12: UF_UNDO_set_mark() - 注意: NX12 UF Undo支持有限
     * - NX1980+: NXOpen::Session::SetUndoMark()
     */
    int createUndoMark(const std::string& markName);

    /**
     * @brief 回滚到指定Undo标记
     * @param markId 标记ID
     * @return true=成功回滚
     *
     * 【NX API】
     * - NX1980+: NXOpen::Session::UndoToMark()
     */
    bool rollbackToMark(int markId);

    /**
     * @brief 删除Undo标记(构建成功后调用, 释放资源)
     */
    void deleteUndoMark(int markId);

    /* ---- 回调注册 ---- */

    /**
     * @brief 注册参数变更回调
     * @param callback 回调函数
     * @return 回调ID (用于注销)
     */
    int registerChangeCallback(ParamChangeCallback callback);

    /**
     * @brief 注销回调
     */
    void unregisterChangeCallback(int callbackId);

    /* ---- 依赖图管理 ---- */

    /**
     * @brief 获取指定参数变更后需要重建的模块列表
     * @param paramId 变更的参数ID
     * @return 受影响的模块名称列表(已按拓扑排序)
     */
    std::vector<std::string> getAffectedModules(const std::string& paramId) const;

    /**
     * @brief 获取构建上下文
     */
    BuildContext& buildContext() { return m_buildCtx; }

private:
    HvacParameterEngine() = default;
    ~HvacParameterEngine() = default;
    HvacParameterEngine(const HvacParameterEngine&) = delete;
    HvacParameterEngine& operator=(const HvacParameterEngine&) = delete;

    /* ---- 内部方法 ---- */

    /** 注册参数→模块依赖关系 */
    void registerDependencies();

    /** 拓扑排序依赖图 */
    void topologicalSort();

    /** 将paramId映射到Level-1结构体成员并赋值 */
    bool applyValueToParam(const std::string& paramId, double value, double& oldValue);

    /** 通知所有注册的回调 */
    void notifyCallbacks(const ParamChangeEvent& event);

    /* ---- 成员变量 ---- */
    bool m_initialized = false;
    HvacParameterSet m_params;
    BuildContext m_buildCtx;

    // 依赖图
    std::map<std::string, DependencyNode> m_dependencyGraph;
    std::vector<std::string> m_sortedBuildOrder;  // 拓扑排序结果

    // 回调管理
    std::map<int, ParamChangeCallback> m_callbacks;
    int m_nextCallbackId = 1;

    // Undo管理
    std::vector<int> m_undoMarks;
    int m_currentUndoMark = -1;
};

#endif /* HVAC_PARAMETER_ENGINE_H */
