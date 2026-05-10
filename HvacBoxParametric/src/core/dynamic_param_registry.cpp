/**
 * @file dynamic_param_registry.cpp
 * @brief 动态参数注册表实现
 * @version 2.0
 * @date 2026-05-10
 */

#include "core/dynamic_param_registry.h"
#include "modules/barrel_door_strategy.h"
#include "modules/mode_disc_strategy.h"
#include "utils/error_handler.h"

/* ============================================================
 * 配置初始化
 * ============================================================ */

void DynamicParamRegistry::configure(const HvacTopologyConfig& config)
{
    m_config = config;
    m_params.clear();
    m_groups.clear();

    HvacErrorHandler::logInfo("DynamicParamRegistry: Configuring for [" +
                              config.getSummary() + "]");

    // 按顺序注册各组参数
    registerBaseParams();
    registerTempDoorParams();
    registerModeDoorParams();
    registerHeaterParams();
    registerEnergyParams();

    HvacErrorHandler::logInfo("DynamicParamRegistry: Total " +
                              std::to_string(getParamCount()) + " params registered in " +
                              std::to_string(m_groups.size()) + " groups.");
}

/* ============================================================
 * 参数查询
 * ============================================================ */

std::vector<DynamicParamEntry> DynamicParamRegistry::getAllParams() const
{
    std::vector<DynamicParamEntry> result;
    result.reserve(m_params.size());
    for (const auto& kv : m_params) {
        result.push_back(kv.second);
    }
    return result;
}

std::vector<DynamicParamEntry> DynamicParamRegistry::getParamsByGroup(
    const std::string& groupId) const
{
    std::vector<DynamicParamEntry> result;
    for (const auto& grp : m_groups) {
        if (grp.groupId == groupId) {
            for (const auto& pid : grp.paramIds) {
                auto it = m_params.find(pid);
                if (it != m_params.end()) {
                    result.push_back(it->second);
                }
            }
            break;
        }
    }
    return result;
}

std::vector<ParamGroup> DynamicParamRegistry::getParamGroups() const
{
    return m_groups;
}

const DynamicParamEntry* DynamicParamRegistry::getParam(const std::string& paramId) const
{
    auto it = m_params.find(paramId);
    return (it != m_params.end()) ? &(it->second) : nullptr;
}

bool DynamicParamRegistry::hasParam(const std::string& paramId) const
{
    return m_params.find(paramId) != m_params.end();
}

/* ============================================================
 * 参数值操作
 * ============================================================ */

ParamStatus DynamicParamRegistry::setParamValue(const std::string& paramId, double value)
{
    auto it = m_params.find(paramId);
    if (it == m_params.end()) return ParamStatus::FAIL_CONFLICT;

    DynamicParamEntry& entry = it->second;
    if (entry.isReadOnly) return ParamStatus::FAIL_CONFLICT;

    // 范围校验
    if (value < entry.minVal || value > entry.maxVal) {
        return ParamStatus::FAIL_OUT_RANGE;
    }

    // 接近边界警告(90%范围外)
    double range = entry.maxVal - entry.minVal;
    double margin = range * 0.1;
    if (value < (entry.minVal + margin) || value > (entry.maxVal - margin)) {
        entry.currentVal = value;
        return ParamStatus::WARN_NEAR_LIMIT;
    }

    entry.currentVal = value;
    return ParamStatus::VALID;
}

double DynamicParamRegistry::getParamValue(const std::string& paramId) const
{
    auto it = m_params.find(paramId);
    return (it != m_params.end()) ? it->second.currentVal : 0.0;
}

void DynamicParamRegistry::resetToDefaults()
{
    for (auto& kv : m_params) {
        kv.second.currentVal = kv.second.defaultVal;
    }
}

std::map<std::string, double> DynamicParamRegistry::exportAsMap() const
{
    std::map<std::string, double> result;
    for (const auto& kv : m_params) {
        result[kv.first] = kv.second.currentVal;
    }
    return result;
}

std::map<std::string, double> DynamicParamRegistry::exportForStrategy(
    const std::string& strategyPrefix) const
{
    std::map<std::string, double> result;
    size_t prefixLen = strategyPrefix.length();

    for (const auto& kv : m_params) {
        if (kv.first.length() > prefixLen &&
            kv.first.substr(0, prefixLen) == strategyPrefix) {
            // 去掉前缀
            std::string key = kv.first.substr(prefixLen);
            result[key] = kv.second.currentVal;
        }
    }
    return result;
}

/* ============================================================
 * 内部: 注册基础参数 (所有配置共用)
 * ============================================================ */

void DynamicParamRegistry::registerBaseParams()
{
    addGroup("base_envelope", "Box Envelope", 10);
    addGroup("base_shell", "Shell & Mold", 20);
    addGroup("base_outlets", "Outlet Positions", 30);

    // --- 箱体包络 ---
    addParam("boxLengthX", "Box Length (X)", "mm", 350, 500, 420, "base_envelope",
             "", "Total box length in X direction");
    addParam("boxWidthY", "Box Width (Y)", "mm", 280, 400, 320, "base_envelope",
             "", "Total box width in Y direction");
    addParam("boxHeightZ", "Box Height (Z)", "mm", 200, 320, 260, "base_envelope",
             "", "Total box height in Z direction");

    // --- 壳体/模具 ---
    addParam("wallThickness", "Wall Thickness", "mm", 2.0, 3.5, 2.5, "base_shell",
             "", "Shell nominal wall thickness (PP+TD20)");
    addParam("partingPlaneZ", "Parting Plane Z", "mm", 80, 192, 130, "base_shell",
             "", "Main parting plane Z height (40-60% of box height)");
    addParam("sealGrooveWidth", "Seal Groove Width", "mm", 3.0, 5.0, 4.0, "base_shell",
             "", "Upper/lower shell seal groove width");

    // --- 出风口 ---
    addParam("defOutletZ", "DEF Outlet Z", "mm", 180, 290, 230, "base_outlets",
             "", "Defrost outlet center height");
    addParam("faceOutletZ", "FACE Outlet Z", "mm", 100, 220, 170, "base_outlets",
             "", "Face outlet center height");
    addParam("footOutletZ", "FOOT Outlet Z", "mm", 20, 60, 40, "base_outlets",
             "", "Foot outlet center height");
}

/* ============================================================
 * 内部: 注册温度风门参数 (按类型和温区数)
 * ============================================================ */

void DynamicParamRegistry::registerTempDoorParams()
{
    // 获取温区后缀列表
    std::vector<std::string> suffixes = getZoneSuffixes();

    // 根据风门类型选择策略并获取参数定义
    std::vector<ParamDefinition> strategyParams;

    switch (m_config.tempDoorType) {
        case TempDoorType::BARREL: {
            BarrelDoorStrategy strategy;
            strategyParams = strategy.getRequiredParams();
            break;
        }
        case TempDoorType::SECTOR:
        case TempDoorType::SINGLE_ROTARY:
        case TempDoorType::BUTTERFLY:
        case TempDoorType::SLIDING:
            // Phase-3/4: 其他策略的参数注册
            // 暂时使用通用旋转门参数
            strategyParams = {
                {"doorRadius", "Door Radius", "mm", 40.0, 120.0, 70.0, "Rotation radius"},
                {"doorSweepAngle", "Sweep Angle", "deg", 45.0, 120.0, 80.0, "Rotation range"},
                {"doorLength", "Door Length", "mm", 80.0, 200.0, 140.0, "Door axial length"},
                {"axisCenterX", "Axis X", "mm", 100.0, 350.0, 200.0, "Rotation axis X"},
                {"axisCenterZ", "Axis Z", "mm", 100.0, 280.0, 180.0, "Rotation axis Z"},
            };
            break;
    }

    // 为每个温区创建一组参数(带后缀)
    for (const auto& suffix : suffixes) {
        std::string groupId = "tempDoor_" + suffix;
        std::string groupName = "Temp Door (" + getZoneDisplayName(suffix) + ")";
        addGroup(groupId, groupName, 40 + static_cast<int>(suffix[0]));

        std::string prefix = "td_" + suffix + "_";

        for (const auto& pd : strategyParams) {
            std::string paramId = prefix + pd.id;
            std::string displayName = getZoneDisplayName(suffix) + " " + pd.name;

            addParam(paramId, displayName, pd.unit, pd.minVal, pd.maxVal,
                     pd.defaultVal, groupId, "TempDoor_" + suffix, pd.description);
        }
    }
}

/* ============================================================
 * 内部: 注册模式门参数
 * ============================================================ */

void DynamicParamRegistry::registerModeDoorParams()
{
    addGroup("modeDoor", "Mode Door", 60);

    std::vector<ParamDefinition> strategyParams;

    switch (m_config.modeDoorType) {
        case ModeDoorType::MODE_DISC: {
            ModeDiscStrategy strategy;
            strategyParams = strategy.getRequiredParams();
            break;
        }
        case ModeDoorType::ROTARY_PLATES:
        case ModeDoorType::LINKED_PLATES:
            // Phase-4: 其他模式门策略参数
            strategyParams = {
                {"modeDoorRadius", "Mode Door Radius", "mm", 30.0, 100.0, 60.0, ""},
                {"modeDoorCount", "Number of Doors", "", 2.0, 4.0, 3.0, ""},
            };
            break;
    }

    std::string prefix = "md_";
    for (const auto& pd : strategyParams) {
        addParam(prefix + pd.id, pd.name, pd.unit, pd.minVal, pd.maxVal,
                 pd.defaultVal, "modeDoor", "ModeDisc", pd.description);
    }
}

/* ============================================================
 * 内部: 注册加热器参数
 * ============================================================ */

void DynamicParamRegistry::registerHeaterParams()
{
    addGroup("heater", "Heater Core", 70);

    // 加热器姿态角(竖置=0°, 但保留可配置)
    double defaultAngle = 0.0;
    switch (m_config.heaterLayout) {
        case HeaterLayout::VERTICAL:   defaultAngle = 0.0; break;
        case HeaterLayout::INCLINED:   defaultAngle = 55.0; break;
        case HeaterLayout::HORIZONTAL: defaultAngle = 85.0; break;
    }

    addParam("heaterTiltAngle", "Heater Tilt Angle", "deg", 0.0, 90.0,
             defaultAngle, "heater", "", "Heater core installation angle");
    addParam("coreSpacing", "Evap-Heater Spacing", "mm", 60.0, 120.0, 90.0,
             "heater", "", "Distance between evaporator and heater center");
    addParam("evapTiltAngle", "Evaporator Tilt", "deg", 0.0, 15.0, 5.0,
             "heater", "", "Evaporator tilt for condensate drainage");
}

/* ============================================================
 * 内部: 注册新能源参数
 * ============================================================ */

void DynamicParamRegistry::registerEnergyParams()
{
    if (m_config.heatSource == HeatSourceType::COOLANT_HEATER &&
        !m_config.hasPTC && !m_config.hasHeatPump) {
        return; // 传统燃油车不需要额外参数
    }

    addGroup("energy", "Energy / EV", 80);

    if (m_config.hasPTC || m_config.heatSource == HeatSourceType::PTC_HEATER) {
        addParam("ptcPower", "PTC Power", "kW", 1.0, 8.0, 4.0,
                 "energy", "", "PTC heater rated power");
        addParam("ptcPosition", "PTC Position", "mm", 50.0, 300.0, 150.0,
                 "energy", "", "PTC module installation X position");
    }

    if (m_config.hasHeatPump || m_config.heatSource == HeatSourceType::HEAT_PUMP) {
        addParam("hpCondensorArea", "HP Condensor Area", "cm2", 100.0, 500.0, 250.0,
                 "energy", "", "Heat pump indoor condenser area");
        addParam("hpRefrigerantPort", "Refrigerant Port Dia", "mm", 8.0, 16.0, 12.0,
                 "energy", "", "Refrigerant pipe connection diameter");
    }
}

/* ============================================================
 * 辅助方法
 * ============================================================ */

void DynamicParamRegistry::addParam(const std::string& id, const std::string& displayName,
                                    const std::string& unit, double minVal, double maxVal,
                                    double defaultVal, const std::string& group,
                                    const std::string& source, const std::string& desc)
{
    DynamicParamEntry entry;
    entry.id = id;
    entry.displayName = displayName;
    entry.unit = unit;
    entry.minVal = minVal;
    entry.maxVal = maxVal;
    entry.defaultVal = defaultVal;
    entry.currentVal = defaultVal;
    entry.group = group;
    entry.sourceStrategy = source;
    entry.description = desc;

    m_params[id] = entry;

    // 将参数ID加入对应分组
    for (auto& grp : m_groups) {
        if (grp.groupId == group) {
            grp.paramIds.push_back(id);
            return;
        }
    }
}

void DynamicParamRegistry::addGroup(const std::string& id, const std::string& name, int order)
{
    ParamGroup grp;
    grp.groupId = id;
    grp.displayName = name;
    grp.order = order;
    m_groups.push_back(grp);
}

std::vector<std::string> DynamicParamRegistry::getZoneSuffixes() const
{
    switch (m_config.zoneType) {
        case ZoneType::DUAL_ZONE:
            return {"L", "R"};
        case ZoneType::TRI_ZONE:
            return {"L", "R", "Rear"};
        case ZoneType::QUAD_ZONE:
            return {"FL", "FR", "RL", "RR"};
        default:
            return {"L", "R"};
    }
}

std::string DynamicParamRegistry::getZoneDisplayName(const std::string& suffix) const
{
    if (suffix == "L") return "Left";
    if (suffix == "R") return "Right";
    if (suffix == "Rear") return "Rear";
    if (suffix == "FL") return "Front-Left";
    if (suffix == "FR") return "Front-Right";
    if (suffix == "RL") return "Rear-Left";
    if (suffix == "RR") return "Rear-Right";
    return suffix;
}

/* ============================================================
 * JSON序列化(简化)
 * ============================================================ */

std::string DynamicParamRegistry::toJson() const
{
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"_topology\": \"" << m_config.getSummary() << "\",\n";
    oss << "  \"_paramCount\": " << getParamCount() << ",\n";

    bool first = true;
    for (const auto& kv : m_params) {
        if (!first) oss << ",\n";
        first = false;
        oss << "  \"" << kv.first << "\": " << kv.second.currentVal;
    }
    oss << "\n}\n";
    return oss.str();
}

bool DynamicParamRegistry::fromJson(const std::string& jsonStr)
{
    // 简化解析: 逐行查找 "key": value
    std::istringstream iss(jsonStr);
    std::string line;

    while (std::getline(iss, line)) {
        auto colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(0, colonPos);
        std::string valStr = line.substr(colonPos + 1);

        // 清理
        key.erase(std::remove(key.begin(), key.end(), '"'), key.end());
        key.erase(std::remove(key.begin(), key.end(), ' '), key.end());
        valStr.erase(std::remove(valStr.begin(), valStr.end(), ','), valStr.end());
        valStr.erase(std::remove(valStr.begin(), valStr.end(), ' '), valStr.end());

        if (key.empty() || key[0] == '_') continue; // 跳过注释字段

        try {
            double val = std::stod(valStr);
            if (hasParam(key)) {
                setParamValue(key, val);
            }
        } catch (...) {}
    }

    return true;
}
