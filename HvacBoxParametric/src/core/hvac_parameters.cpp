/**
 * @file hvac_parameters.cpp
 * @brief HVAC参数集合管理器实现
 * @version 1.0
 * @date 2026-05-10
 */

#include "hvac_parameters.h"
#include <cmath>

/* ============================================================
 * Level-1 参数校验
 * ============================================================ */

std::vector<std::pair<std::string, ParamStatus>>
HvacParameterSet::validateLevel1() const
{
    std::vector<std::pair<std::string, ParamStatus>> failures;
    const auto& reg = HvacParamRangeRegistry::instance();

    auto check = [&](const std::string& id, double val) {
        ParamStatus st = reg.validate(id, val);
        if (st != ParamStatus::VALID) {
            failures.emplace_back(id, st);
        }
    };

    check("L1_001_boxLengthX",      m_level1.boxLengthX);
    check("L1_002_boxWidthY",       m_level1.boxWidthY);
    check("L1_003_boxHeightZ",      m_level1.boxHeightZ);
    check("L1_004_evapTiltAngle",   m_level1.evapTiltAngle);
    check("L1_005_heaterTiltAngle", m_level1.heaterTiltAngle);
    check("L1_006_coreSpacing",     m_level1.coreSpacing);
    check("L1_007_partingPlaneZ",   m_level1.partingPlaneZ);
    check("L1_008_wallThickness",   m_level1.wallThickness);
    check("L1_009_sealGrooveWidth", m_level1.sealGrooveWidth);
    check("L1_010_doorAxisToWall",  m_level1.doorAxisToWall);
    check("L1_011_defOutletZ",      m_level1.defOutletZ);
    check("L1_012_faceOutletZ",     m_level1.faceOutletZ);
    check("L1_013_footOutletZ",     m_level1.footOutletZ);
    check("L1_014_tempDoorCenterX", m_level1.tempDoorCenterX);
    check("L1_015_tempDoorCenterZ", m_level1.tempDoorCenterZ);
    check("L1_016_modeDoorCenterX", m_level1.modeDoorCenterX);
    check("L1_017_actuatorOffsetY", m_level1.actuatorOffsetY);

    return failures;
}

ParamStatus HvacParameterSet::validateSingleParam(
    const std::string& paramId, double value) const
{
    return HvacParamRangeRegistry::instance().validate(paramId, value);
}

/* ============================================================
 * Level-2 自动计算主入口
 * ============================================================ */

void HvacParameterSet::recalculateLevel2()
{
    calcChamberSizes();
    calcTempDoorGeometry();
    calcLinkageKinematics();
    calcSealAndSnaps();
    calcRibParameters();
}

/* ============================================================
 * Level-2 子计算: 腔室尺寸
 * ============================================================ */

void HvacParameterSet::calcChamberSizes()
{
    // 芯体间隙: 每侧1.5~3.0mm, 取2.0mm标准值
    constexpr double CORE_CLEARANCE = 2.0;

    // 蒸发器腔室 = 芯体尺寸 + 2×间隙
    m_level2.evapChamberW = m_level0.evapWidth + 2.0 * CORE_CLEARANCE;
    m_level2.evapChamberH = m_level0.evapHeight + 2.0 * CORE_CLEARANCE;
    m_level2.evapChamberD = m_level0.evapDepth + 2.0 * CORE_CLEARANCE;

    // 加热器腔室
    m_level2.heaterChamberW = m_level0.heaterWidth + 2.0 * CORE_CLEARANCE;
    m_level2.heaterChamberH = m_level0.heaterHeight + 2.0 * CORE_CLEARANCE;
    m_level2.heaterChamberD = m_level0.heaterDepth + 2.0 * CORE_CLEARANCE;
}

/* ============================================================
 * Level-2 子计算: 温度风门几何
 * ============================================================ */

void HvacParameterSet::calcTempDoorGeometry()
{
    // 映射规则 T-01: 温度风门旋转半径
    // R = (Heater_H / 2) / cos(heaterTiltAngle) + clearance(2mm)
    double heaterHalfH = m_level0.heaterHeight / 2.0;
    double cosAngle = std::cos(degToRad(m_level1.heaterTiltAngle));

    if (std::abs(cosAngle) < 0.01) {
        cosAngle = 0.01; // 防止除零(极端倾角保护)
    }
    m_level2.tempDoorRadius = heaterHalfH / cosAngle + 2.0;

    // 映射规则 T-02: 摆动角度
    // θ_sweep = arctan(H*sin(angle)/spacing) + margin(5°)
    double sinAngle = std::sin(degToRad(m_level1.heaterTiltAngle));
    double numerator = m_level0.heaterHeight * sinAngle;
    double denominator = m_level1.coreSpacing;

    if (denominator < 1.0) denominator = 1.0; // 防止除零
    m_level2.tempDoorSweepAngle = radToDeg(std::atan(numerator / denominator)) + 5.0;

    // 限制在合理范围
    if (m_level2.tempDoorSweepAngle < HvacConst::TEMP_DOOR_SWEEP_MIN) {
        m_level2.tempDoorSweepAngle = HvacConst::TEMP_DOOR_SWEEP_MIN;
    }
    if (m_level2.tempDoorSweepAngle > HvacConst::TEMP_DOOR_SWEEP_MAX) {
        m_level2.tempDoorSweepAngle = HvacConst::TEMP_DOOR_SWEEP_MAX;
    }

    // 有效弧长 = π × R × (θ/360)
    m_level2.tempDoorArcLength = HvacConst::HVAC_PI * m_level2.tempDoorRadius
                                 * (m_level2.tempDoorSweepAngle / 360.0);

    // 风门轴长度 = 箱体内宽 - 2×轴承座宽(每侧8mm)
    constexpr double BEARING_SEAT_WIDTH = 8.0;
    double innerWidth = m_level1.boxWidthY - 2.0 * m_level1.wallThickness;
    m_level2.tempDoorShaftLength = innerWidth - 2.0 * BEARING_SEAT_WIDTH;
}

/* ============================================================
 * Level-2 子计算: 连杆运动学
 * ============================================================ */

void HvacParameterSet::calcLinkageKinematics()
{
    // 简化四连杆求解 (Phase-1使用经验公式, Phase-3替换为完整Newton-Raphson)
    //
    // 执行器输出轴位置: 箱体侧壁外偏移actuatorOffsetY
    // 风门轴位置: tempDoorCenterX, tempDoorCenterZ
    //
    // 曲柄长度 ≈ 执行器行程角对应的有效臂长
    // 典型执行器旋转90°, 风门摆动θ_sweep

    double actuatorArmRadius = 20.0; // 执行器输出臂半径 (典型值mm)
    double transmissionRatio = m_level2.tempDoorSweepAngle / 90.0;

    // 简化: 连杆长度 ≈ 风门轴到执行器轴的距离 × 系数
    double dx = m_level1.boxWidthY / 2.0 + m_level1.actuatorOffsetY;
    double dz = 30.0; // 执行器轴与风门轴的Z向偏差(典型值)

    double axisDistance = std::sqrt(dx * dx + dz * dz);

    m_level2.crankLength = actuatorArmRadius;
    m_level2.rockerLength = actuatorArmRadius * transmissionRatio;
    m_level2.couplerLength = axisDistance - m_level2.crankLength - m_level2.rockerLength;

    if (m_level2.couplerLength < 10.0) {
        m_level2.couplerLength = 10.0; // 最小连杆长度保护
    }

    // 铰接点坐标 (简化放置)
    double wallY = m_level1.boxWidthY / 2.0;
    m_level2.linkagePivotP1 = {m_level1.tempDoorCenterX,
                                wallY + m_level1.actuatorOffsetY,
                                m_level1.tempDoorCenterZ + 30.0};
    m_level2.linkagePivotP2 = {m_level1.tempDoorCenterX,
                                wallY,
                                m_level1.tempDoorCenterZ};
}

/* ============================================================
 * Level-2 子计算: 密封面与卡扣
 * ============================================================ */

void HvacParameterSet::calcSealAndSnaps()
{
    // 分型面周长 ≈ 2×(长+宽) (简化为矩形)
    double partingPerimeter = 2.0 * (m_level1.boxLengthX + m_level1.boxWidthY);

    // 功能口周长 (蒸发器口 + 出风口×3)
    double funcPortPerimeter = 2.0 * (m_level0.evapWidth + m_level0.evapHeight)
                             + 3.0 * 200.0; // 每个出风口约200mm周长(简化)

    m_level2.sealTotalLength = partingPerimeter + funcPortPerimeter;

    // 卡扣分布: 沿分型面等距布置
    double snapSpacing = 80.0; // 目标间距80mm (在60-100mm规范范围内)
    int snapCount = static_cast<int>(partingPerimeter / snapSpacing);
    if (snapCount < 8) snapCount = 8; // 至少8个卡扣

    m_level2.snapFitPositions.clear();
    m_level2.snapFitPositions.reserve(snapCount);

    // 沿矩形分型面均匀分布卡扣坐标
    double halfL = m_level1.boxLengthX / 2.0;
    double halfW = m_level1.boxWidthY / 2.0;
    double z = m_level1.partingPlaneZ;
    double step = partingPerimeter / static_cast<double>(snapCount);

    double accumulated = 0.0;
    for (int i = 0; i < snapCount; ++i) {
        double pos = i * step;
        Vec3d pt;
        pt.z = z;

        if (pos < m_level1.boxLengthX) {
            // 底边 (沿X正方向)
            pt.x = -halfL + pos;
            pt.y = -halfW;
        } else if (pos < m_level1.boxLengthX + m_level1.boxWidthY) {
            // 右边 (沿Y正方向)
            pt.x = halfL;
            pt.y = -halfW + (pos - m_level1.boxLengthX);
        } else if (pos < 2.0 * m_level1.boxLengthX + m_level1.boxWidthY) {
            // 顶边 (沿X负方向)
            pt.x = halfL - (pos - m_level1.boxLengthX - m_level1.boxWidthY);
            pt.y = halfW;
        } else {
            // 左边 (沿Y负方向)
            pt.x = -halfL;
            pt.y = halfW - (pos - 2.0 * m_level1.boxLengthX - m_level1.boxWidthY);
        }

        m_level2.snapFitPositions.push_back(pt);
    }
}

/* ============================================================
 * Level-2 子计算: 加强筋参数
 * ============================================================ */

void HvacParameterSet::calcRibParameters()
{
    // 加强筋高度 = 壁厚 × 0.6 (DFM准则: 0.5~0.7倍壁厚)
    m_level2.ribHeight = m_level1.wallThickness * 0.6;

    // 加强筋间距 = 40mm (DFM准则: 30~50mm)
    m_level2.ribSpacing = 40.0;

    // 主拔模方向: Z轴正方向 (开模方向=上)
    m_level2.draftDirection = {0.0, 0.0, 1.0};
}

/* ============================================================
 * JSON序列化 (简化实现, 不依赖第三方库)
 * ============================================================ */

bool HvacParameterSet::loadFromJson(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    // 简化JSON解析: 逐行读取 "key": value 格式
    // 生产环境建议替换为 nlohmann/json 或 rapidjson
    std::string line;
    while (std::getline(file, line)) {
        // 跳过空行和注释
        if (line.empty() || line[0] == '/' || line[0] == '#') continue;

        // 查找 "key": value 模式
        auto colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(0, colonPos);
        std::string valStr = line.substr(colonPos + 1);

        // 清理引号和空格
        key.erase(std::remove(key.begin(), key.end(), '"'), key.end());
        key.erase(std::remove(key.begin(), key.end(), ' '), key.end());
        valStr.erase(std::remove(valStr.begin(), valStr.end(), ','), valStr.end());
        valStr.erase(std::remove(valStr.begin(), valStr.end(), ' '), valStr.end());

        if (valStr.empty()) continue;

        try {
            double val = std::stod(valStr);

            // Level-0
            if (key == "evapWidth") m_level0.evapWidth = val;
            else if (key == "evapHeight") m_level0.evapHeight = val;
            else if (key == "evapDepth") m_level0.evapDepth = val;
            else if (key == "heaterWidth") m_level0.heaterWidth = val;
            else if (key == "heaterHeight") m_level0.heaterHeight = val;
            else if (key == "heaterDepth") m_level0.heaterDepth = val;
            else if (key == "blowerDiameter") m_level0.blowerDiameter = val;
            else if (key == "coolingCapacity") m_level0.coolingCapacity = val;
            else if (key == "heatingCapacity") m_level0.heatingCapacity = val;
            else if (key == "maxAirflow") m_level0.maxAirflow = val;

            // Level-1
            else if (key == "boxLengthX") m_level1.boxLengthX = val;
            else if (key == "boxWidthY") m_level1.boxWidthY = val;
            else if (key == "boxHeightZ") m_level1.boxHeightZ = val;
            else if (key == "evapTiltAngle") m_level1.evapTiltAngle = val;
            else if (key == "heaterTiltAngle") m_level1.heaterTiltAngle = val;
            else if (key == "coreSpacing") m_level1.coreSpacing = val;
            else if (key == "partingPlaneZ") m_level1.partingPlaneZ = val;
            else if (key == "wallThickness") m_level1.wallThickness = val;
            else if (key == "sealGrooveWidth") m_level1.sealGrooveWidth = val;
            else if (key == "doorAxisToWall") m_level1.doorAxisToWall = val;
            else if (key == "defOutletZ") m_level1.defOutletZ = val;
            else if (key == "faceOutletZ") m_level1.faceOutletZ = val;
            else if (key == "footOutletZ") m_level1.footOutletZ = val;
            else if (key == "tempDoorCenterX") m_level1.tempDoorCenterX = val;
            else if (key == "tempDoorCenterZ") m_level1.tempDoorCenterZ = val;
            else if (key == "modeDoorCenterX") m_level1.modeDoorCenterX = val;
            else if (key == "modeDoorCenterZ") m_level1.modeDoorCenterZ = val;
            else if (key == "actuatorOffsetY") m_level1.actuatorOffsetY = val;
        } catch (...) {
            // 解析失败跳过该行
            continue;
        }
    }

    file.close();
    recalculateLevel2(); // 加载后自动计算Level-2
    return true;
}

bool HvacParameterSet::saveToJson(const std::string& filePath) const
{
    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"_comment\": \"HVAC Box Parametric - Parameter Configuration\",\n";
    file << "  \"_version\": \"1.0\",\n\n";

    // Level-0
    file << "  \"_level0_comment\": \"Vehicle Input Parameters\",\n";
    file << "  \"evapWidth\": " << m_level0.evapWidth << ",\n";
    file << "  \"evapHeight\": " << m_level0.evapHeight << ",\n";
    file << "  \"evapDepth\": " << m_level0.evapDepth << ",\n";
    file << "  \"heaterWidth\": " << m_level0.heaterWidth << ",\n";
    file << "  \"heaterHeight\": " << m_level0.heaterHeight << ",\n";
    file << "  \"heaterDepth\": " << m_level0.heaterDepth << ",\n";
    file << "  \"blowerDiameter\": " << m_level0.blowerDiameter << ",\n";
    file << "  \"coolingCapacity\": " << m_level0.coolingCapacity << ",\n";
    file << "  \"heatingCapacity\": " << m_level0.heatingCapacity << ",\n";
    file << "  \"maxAirflow\": " << m_level0.maxAirflow << ",\n\n";

    // Level-1
    file << "  \"_level1_comment\": \"Design Driver Parameters\",\n";
    file << "  \"boxLengthX\": " << m_level1.boxLengthX << ",\n";
    file << "  \"boxWidthY\": " << m_level1.boxWidthY << ",\n";
    file << "  \"boxHeightZ\": " << m_level1.boxHeightZ << ",\n";
    file << "  \"evapTiltAngle\": " << m_level1.evapTiltAngle << ",\n";
    file << "  \"heaterTiltAngle\": " << m_level1.heaterTiltAngle << ",\n";
    file << "  \"coreSpacing\": " << m_level1.coreSpacing << ",\n";
    file << "  \"partingPlaneZ\": " << m_level1.partingPlaneZ << ",\n";
    file << "  \"wallThickness\": " << m_level1.wallThickness << ",\n";
    file << "  \"sealGrooveWidth\": " << m_level1.sealGrooveWidth << ",\n";
    file << "  \"doorAxisToWall\": " << m_level1.doorAxisToWall << ",\n";
    file << "  \"defOutletZ\": " << m_level1.defOutletZ << ",\n";
    file << "  \"faceOutletZ\": " << m_level1.faceOutletZ << ",\n";
    file << "  \"footOutletZ\": " << m_level1.footOutletZ << ",\n";
    file << "  \"tempDoorCenterX\": " << m_level1.tempDoorCenterX << ",\n";
    file << "  \"tempDoorCenterZ\": " << m_level1.tempDoorCenterZ << ",\n";
    file << "  \"modeDoorCenterX\": " << m_level1.modeDoorCenterX << ",\n";
    file << "  \"modeDoorCenterZ\": " << m_level1.modeDoorCenterZ << ",\n";
    file << "  \"actuatorOffsetY\": " << m_level1.actuatorOffsetY << "\n";

    file << "}\n";
    file.close();
    return true;
}

/* ============================================================
 * NX Expression 同步
 * ============================================================ */

void HvacParameterSet::exportToNxExpressions(tag_t partTag) const
{
    if (partTag == NULL_TAG) return;

    // 使用UF API创建/更新Expression (全版本兼容)
    auto setOrCreateExp = [&](const std::string& name, double value) {
        if (!isValidExpressionName(name)) return;

        std::string expStr = name + " = " + std::to_string(value);

        // 尝试编辑已存在的Expression
        tag_t expTag = NULL_TAG;
        int rc = UF_MODL_ask_exp_tag_string(expStr.c_str(), &expTag);

        if (expTag == NULL_TAG) {
            // 不存在则创建
            UF_MODL_create_exp(expStr.c_str());
        } else {
            // 已存在则更新
            UF_MODL_edit_exp(expStr.c_str());
        }
    };

    setOrCreateExp("HVAC_L1_boxLengthX", m_level1.boxLengthX);
    setOrCreateExp("HVAC_L1_boxWidthY", m_level1.boxWidthY);
    setOrCreateExp("HVAC_L1_boxHeightZ", m_level1.boxHeightZ);
    setOrCreateExp("HVAC_L1_evapTiltAngle", m_level1.evapTiltAngle);
    setOrCreateExp("HVAC_L1_heaterTiltAngle", m_level1.heaterTiltAngle);
    setOrCreateExp("HVAC_L1_coreSpacing", m_level1.coreSpacing);
    setOrCreateExp("HVAC_L1_partingPlaneZ", m_level1.partingPlaneZ);
    setOrCreateExp("HVAC_L1_wallThickness", m_level1.wallThickness);
    setOrCreateExp("HVAC_L1_sealGrooveWidth", m_level1.sealGrooveWidth);
    setOrCreateExp("HVAC_L1_doorAxisToWall", m_level1.doorAxisToWall);
    setOrCreateExp("HVAC_L1_defOutletZ", m_level1.defOutletZ);
    setOrCreateExp("HVAC_L1_faceOutletZ", m_level1.faceOutletZ);
    setOrCreateExp("HVAC_L1_footOutletZ", m_level1.footOutletZ);
    setOrCreateExp("HVAC_L1_tempDoorCenterX", m_level1.tempDoorCenterX);
    setOrCreateExp("HVAC_L1_tempDoorCenterZ", m_level1.tempDoorCenterZ);
    setOrCreateExp("HVAC_L1_modeDoorCenterX", m_level1.modeDoorCenterX);
    setOrCreateExp("HVAC_L1_actuatorOffsetY", m_level1.actuatorOffsetY);

    // 刷新模型
    UF_MODL_update();
}

void HvacParameterSet::importFromNxExpressions(tag_t partTag)
{
    if (partTag == NULL_TAG) return;

    auto readExp = [&](const std::string& name, double& outValue) {
        tag_t expTag = NULL_TAG;
        // 构造查询字符串
        std::string queryStr = name;
        int rc = UF_MODL_ask_exp_tag_string(queryStr.c_str(), &expTag);
        if (expTag != NULL_TAG) {
            double val = 0.0;
            // UF_MODL_ask_exp_tag_value 获取表达式值
            char expString[256] = {0};
            UF_MODL_ask_exp_tag_string2(expTag, expString);
            // 从 "name = value" 格式解析值
            std::string s(expString);
            auto eqPos = s.find('=');
            if (eqPos != std::string::npos) {
                try {
                    outValue = std::stod(s.substr(eqPos + 1));
                } catch (...) {}
            }
        }
    };

    readExp("HVAC_L1_boxLengthX", m_level1.boxLengthX);
    readExp("HVAC_L1_boxWidthY", m_level1.boxWidthY);
    readExp("HVAC_L1_boxHeightZ", m_level1.boxHeightZ);
    readExp("HVAC_L1_evapTiltAngle", m_level1.evapTiltAngle);
    readExp("HVAC_L1_heaterTiltAngle", m_level1.heaterTiltAngle);
    readExp("HVAC_L1_coreSpacing", m_level1.coreSpacing);
    readExp("HVAC_L1_partingPlaneZ", m_level1.partingPlaneZ);
    readExp("HVAC_L1_wallThickness", m_level1.wallThickness);
    readExp("HVAC_L1_sealGrooveWidth", m_level1.sealGrooveWidth);
    readExp("HVAC_L1_doorAxisToWall", m_level1.doorAxisToWall);
    readExp("HVAC_L1_defOutletZ", m_level1.defOutletZ);
    readExp("HVAC_L1_faceOutletZ", m_level1.faceOutletZ);
    readExp("HVAC_L1_footOutletZ", m_level1.footOutletZ);
    readExp("HVAC_L1_tempDoorCenterX", m_level1.tempDoorCenterX);
    readExp("HVAC_L1_tempDoorCenterZ", m_level1.tempDoorCenterZ);
    readExp("HVAC_L1_modeDoorCenterX", m_level1.modeDoorCenterX);
    readExp("HVAC_L1_actuatorOffsetY", m_level1.actuatorOffsetY);

    recalculateLevel2();
}
