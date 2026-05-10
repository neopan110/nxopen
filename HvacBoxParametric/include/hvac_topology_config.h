/**
 * @file hvac_topology_config.h
 * @brief HVAC箱体参数化系统 - 结构拓扑配置层
 * @details 在参数赋值之前确定箱体的"骨架形式"，一旦选定，
 *          约束后续所有参数的含义、数量和模块构建逻辑。
 *
 * 【设计哲学】
 * 结构拓扑 ≠ 尺寸参数。拓扑决定"有什么"，参数决定"多大多长"。
 * 例如：选了"桶型风门"后，参数集自动出现barrelRadius/barrelArcAngle；
 *       选了"单片旋转"后，参数集变为plateWidth/plateHeight。
 *
 * 【使用流程】
 * 1. 用户在对话框第一页选择拓扑配置
 * 2. 系统根据配置动态生成参数集(ParamRegistry)
 * 3. 用户在第二页填写尺寸参数
 * 4. 策略工厂根据配置创建对应的Builder实例
 *
 * @version 2.0
 * @date 2026-05-10
 */

#ifndef HVAC_TOPOLOGY_CONFIG_H
#define HVAC_TOPOLOGY_CONFIG_H

#include <string>
#include <vector>

/* ============================================================
 * 箱体布局形式
 * ============================================================ */

/**
 * @enum LayoutType
 * @brief 箱体整体布局形式
 *
 * 布局决定蒸发器、加热器、鼓风机的相对位置关系。
 * 影响: 风道路径、出风口朝向、整车Package适配。
 */
enum class LayoutType {
    CENTER_MOUNT,       ///< 中置式 - 蒸发器居中(Denso典型, 对称气流分配)
    OFFSET_LEFT,        ///< 左偏置 - 蒸发器偏驾驶员侧(Valeo部分欧系车型)
    OFFSET_RIGHT,       ///< 右偏置 - 蒸发器偏副驾侧
    SEMI_CENTER         ///< 半中置 - 微偏(Hanon折中, 兼顾Package和气流)
};

/* ============================================================
 * 温区配置
 * ============================================================ */

/**
 * @enum ZoneType
 * @brief 温度控制区域数量
 *
 * 温区数量直接决定温度风门数量:
 * - 双温区: 左右各1个温度风门, 独立控制
 * - 三温区: 左前/右前/后排各1个
 * - 四温区: 左前/右前/左后/右后各1个
 *
 * 影响: 温度风门数量、混合腔隔板数量、执行器数量
 */
enum class ZoneType {
    DUAL_ZONE,          ///< 双温区 (左/右独立, BBA标配)
    TRI_ZONE,           ///< 三温区 (左前/右前/后排)
    QUAD_ZONE           ///< 四温区 (四区独立, 行政级)
};

/* ============================================================
 * 温度风门形式
 * ============================================================ */

/**
 * @enum TempDoorType
 * @brief 温度混合风门结构形式
 *
 * 决定温度风门的几何形态、运动方式、密封方式。
 * 每种形式有完全不同的参数集和构建策略。
 */
enum class TempDoorType {
    BARREL,             ///< 桶型风门 - 圆柱弧面旋转(Valeo高端, BBA标配, 风量线性最佳)
    SECTOR,             ///< 扇形风门 - 扇形平板绕顶点旋转(日系中高端)
    SINGLE_ROTARY,      ///< 单片旋转 - 矩形平板绕中心轴旋转(经济型, 成本最低)
    BUTTERFLY,          ///< 蝶形对开 - 双片相向旋转(Denso高端, 混合效率高)
    SLIDING             ///< 滑动式 - 平板直线平移(EV薄型化, 电动车专用)
};

/* ============================================================
 * 模式风门形式
 * ============================================================ */

/**
 * @enum ModeDoorType
 * @brief 出风模式切换机构形式
 *
 * 模式盘(MODE_DISC): 一个圆盘控制所有出风模式，结构紧凑，一个执行器。
 * 旋转片(ROTARY_PLATES): 多个独立风门片各自旋转，灵活但复杂。
 * 联动片(LINKED_PLATES): 多片通过连杆联动，一个执行器。
 */
enum class ModeDoorType {
    MODE_DISC,          ///< 模式盘 - 一个圆盘多窗口(你的选择, 最紧凑)
    ROTARY_PLATES,      ///< 旋转片 - 多片独立旋转(灵活, 可自由组合模式)
    LINKED_PLATES       ///< 联动片 - 多片连杆联动(折中方案)
};

/* ============================================================
 * 内循环风门形式
 * ============================================================ */

enum class IntakeDoorType {
    SINGLE_FLAP,        ///< 单片翻转
    DUAL_FLAP           ///< 双片对开
};

/* ============================================================
 * 加热器布局
 * ============================================================ */

/**
 * @enum HeaterLayout
 * @brief 加热器芯体安装姿态
 *
 * 影响: 混合腔形状、温度风门位置、冷凝水路径
 */
enum class HeaterLayout {
    VERTICAL,           ///< 竖置 0° (日系主流, 你的选择)
    INCLINED,           ///< 斜置 30°~75° (欧系主流)
    HORIZONTAL          ///< 卧置 ~90° (紧凑车型)
};

/* ============================================================
 * 新能源适配
 * ============================================================ */

/**
 * @enum HeatSourceType
 * @brief 热源类型
 *
 * 传统燃油车: 发动机冷却液加热器
 * 纯电动车: PTC电加热器 或 热泵系统
 * 混动: 可能两者兼有
 */
enum class HeatSourceType {
    COOLANT_HEATER,     ///< 水暖加热器(传统, 发动机冷却液)
    PTC_HEATER,         ///< PTC电加热器(纯电动, 直接加热空气)
    HEAT_PUMP,          ///< 热泵系统(高效电动, 制冷剂换热)
    HYBRID_PTC_COOLANT  ///< 混合(PTC + 水暖, 混动车型)
};

/* ============================================================
 * 出风模式定义
 * ============================================================ */

/**
 * @enum AirMode
 * @brief HVAC出风模式
 * @details 模式盘通过旋转不同角度切换这些模式
 */
enum class AirMode {
    FACE,               ///< 吹面 - 上半身舒适
    BI_LEVEL,           ///< 双层 - Face + Foot同时
    FOOT,               ///< 暖足 - 下半身加热
    FOOT_DEF,           ///< 暖足+除霜 - Foot + Defrost同时
    DEF,                ///< 除霜 - 挡风玻璃除雾/除冰
    NUM_MODES           ///< 模式总数 (5)
};

/* ============================================================
 * 模式盘窗口定义
 * ============================================================ */

/**
 * @struct DiscWindow
 * @brief 模式盘上的单个窗口几何定义
 */
struct DiscWindow {
    std::string name;       ///< 窗口名称 (如 "DEF_window")
    double startAngle;      ///< 起始角度 (deg, 从盘基准0°起算)
    double spanAngle;       ///< 张角 (deg)
    double innerRadius;     ///< 内圈半径 (mm, 0=从圆心开始)
    double outerRadius;     ///< 外圈半径 (mm)
};

/**
 * @struct ModePosition
 * @brief 模式盘旋转角度与出风模式的映射
 */
struct ModePosition {
    AirMode mode;           ///< 出风模式
    double discAngle;       ///< 盘旋转角度 (deg)
    std::string description;///< 模式描述
};

/* ============================================================
 * 主配置结构体
 * ============================================================ */

/**
 * @struct HvacTopologyConfig
 * @brief HVAC箱体完整拓扑配置
 * @details 用户在对话框第一页选择后生成。
 *          此配置确定后不可在本次构建中变更。
 *
 * 默认值 = 你确认的首选配置:
 *   双温区 + 桶型风门 + 模式盘 + 竖置加热器 + 都考虑新能源
 */
struct HvacTopologyConfig {

    // === 箱体布局 ===
    LayoutType layout = LayoutType::CENTER_MOUNT;

    // === 温区 ===
    ZoneType zoneType = ZoneType::DUAL_ZONE;

    // === 温度风门 ===
    TempDoorType tempDoorType = TempDoorType::BARREL;

    // === 模式风门 ===
    ModeDoorType modeDoorType = ModeDoorType::MODE_DISC;

    // === 内循环风门 ===
    IntakeDoorType intakeDoorType = IntakeDoorType::SINGLE_FLAP;

    // === 加热器 ===
    HeaterLayout heaterLayout = HeaterLayout::VERTICAL;

    // === 热源 ===
    HeatSourceType heatSource = HeatSourceType::COOLANT_HEATER;

    // === 扩展选项 ===
    bool hasPTC = false;            ///< 是否集成PTC辅助加热
    bool hasHeatPump = false;       ///< 是否热泵系统
    int  actuatorCount = 3;         ///< 执行器数量 (温度L+R=2, 模式盘=1, 最少3)
    bool hasAQS = false;            ///< 是否有空气质量传感器
    bool hasIonizer = false;        ///< 是否有离子发生器

    // === 模式盘配置 ===
    int modeCount = static_cast<int>(AirMode::NUM_MODES); ///< 出风模式数量
    double discRotationRange = 120.0; ///< 模式盘总旋转范围 (deg)

    /* ---- 派生查询方法 ---- */

    /** 温度风门数量 (由温区决定) */
    int getTempDoorCount() const {
        switch (zoneType) {
            case ZoneType::DUAL_ZONE: return 2;
            case ZoneType::TRI_ZONE:  return 3;
            case ZoneType::QUAD_ZONE: return 4;
            default: return 2;
        }
    }

    /** 执行器总数 */
    int getTotalActuatorCount() const {
        // 温度风门: 每个温区1个
        // 模式盘: 1个
        // 内循环: 1个
        return getTempDoorCount() + 1 + 1;
    }

    /** 是否需要混合腔隔板 (多温区时需要左右分隔) */
    bool needsMixingBaffle() const {
        return zoneType != ZoneType::DUAL_ZONE || getTempDoorCount() > 1;
    }

    /** 配置摘要字符串 (用于日志和报告) */
    std::string getSummary() const {
        std::string s;
        // Layout
        switch (layout) {
            case LayoutType::CENTER_MOUNT: s += "Center"; break;
            case LayoutType::OFFSET_LEFT:  s += "OffsetL"; break;
            case LayoutType::OFFSET_RIGHT: s += "OffsetR"; break;
            case LayoutType::SEMI_CENTER:  s += "SemiCenter"; break;
        }
        s += " | ";
        // Zone
        switch (zoneType) {
            case ZoneType::DUAL_ZONE: s += "2-Zone"; break;
            case ZoneType::TRI_ZONE:  s += "3-Zone"; break;
            case ZoneType::QUAD_ZONE: s += "4-Zone"; break;
        }
        s += " | ";
        // Temp door
        switch (tempDoorType) {
            case TempDoorType::BARREL:        s += "Barrel"; break;
            case TempDoorType::SECTOR:        s += "Sector"; break;
            case TempDoorType::SINGLE_ROTARY: s += "PlateRotary"; break;
            case TempDoorType::BUTTERFLY:     s += "Butterfly"; break;
            case TempDoorType::SLIDING:       s += "Sliding"; break;
        }
        s += " | ";
        // Mode
        switch (modeDoorType) {
            case ModeDoorType::MODE_DISC:     s += "ModeDisc"; break;
            case ModeDoorType::ROTARY_PLATES: s += "RotaryPlates"; break;
            case ModeDoorType::LINKED_PLATES: s += "LinkedPlates"; break;
        }
        s += " | ";
        // Heater
        switch (heaterLayout) {
            case HeaterLayout::VERTICAL:   s += "Vertical"; break;
            case HeaterLayout::INCLINED:   s += "Inclined"; break;
            case HeaterLayout::HORIZONTAL: s += "Horizontal"; break;
        }
        s += " | ";
        // Heat source
        switch (heatSource) {
            case HeatSourceType::COOLANT_HEATER:    s += "Coolant"; break;
            case HeatSourceType::PTC_HEATER:        s += "PTC"; break;
            case HeatSourceType::HEAT_PUMP:         s += "HeatPump"; break;
            case HeatSourceType::HYBRID_PTC_COOLANT:s += "Hybrid"; break;
        }
        return s;
    }
};

/* ============================================================
 * 预设配置 (快速选择)
 * ============================================================ */

namespace HvacPresets {

    /** 欧系BBA高端配置 (你的首选) */
    inline HvacTopologyConfig europeanPremium() {
        HvacTopologyConfig cfg;
        cfg.layout = LayoutType::CENTER_MOUNT;
        cfg.zoneType = ZoneType::DUAL_ZONE;
        cfg.tempDoorType = TempDoorType::BARREL;
        cfg.modeDoorType = ModeDoorType::MODE_DISC;
        cfg.intakeDoorType = IntakeDoorType::SINGLE_FLAP;
        cfg.heaterLayout = HeaterLayout::VERTICAL;
        cfg.heatSource = HeatSourceType::COOLANT_HEATER;
        cfg.actuatorCount = 4; // 2×temp + 1×mode + 1×intake
        return cfg;
    }

    /** 欧系BBA纯电动版 */
    inline HvacTopologyConfig europeanPremiumEV() {
        HvacTopologyConfig cfg = europeanPremium();
        cfg.heatSource = HeatSourceType::HEAT_PUMP;
        cfg.hasHeatPump = true;
        cfg.hasPTC = true; // PTC辅助低温启动
        return cfg;
    }

    /** 日系中高端 */
    inline HvacTopologyConfig japaneseMidHigh() {
        HvacTopologyConfig cfg;
        cfg.layout = LayoutType::CENTER_MOUNT;
        cfg.zoneType = ZoneType::DUAL_ZONE;
        cfg.tempDoorType = TempDoorType::SECTOR;
        cfg.modeDoorType = ModeDoorType::MODE_DISC;
        cfg.heaterLayout = HeaterLayout::VERTICAL;
        cfg.heatSource = HeatSourceType::COOLANT_HEATER;
        return cfg;
    }

    /** 经济型车 */
    inline HvacTopologyConfig economyCar() {
        HvacTopologyConfig cfg;
        cfg.layout = LayoutType::CENTER_MOUNT;
        cfg.zoneType = ZoneType::DUAL_ZONE;
        cfg.tempDoorType = TempDoorType::SINGLE_ROTARY;
        cfg.modeDoorType = ModeDoorType::ROTARY_PLATES;
        cfg.heaterLayout = HeaterLayout::INCLINED;
        cfg.heatSource = HeatSourceType::COOLANT_HEATER;
        return cfg;
    }

    /** 行政级轿车 (三温区) */
    inline HvacTopologyConfig executiveSedan() {
        HvacTopologyConfig cfg;
        cfg.layout = LayoutType::CENTER_MOUNT;
        cfg.zoneType = ZoneType::TRI_ZONE;
        cfg.tempDoorType = TempDoorType::BARREL;
        cfg.modeDoorType = ModeDoorType::MODE_DISC;
        cfg.heaterLayout = HeaterLayout::VERTICAL;
        cfg.heatSource = HeatSourceType::COOLANT_HEATER;
        cfg.actuatorCount = 5; // 3×temp + 1×mode + 1×intake
        return cfg;
    }

} // namespace HvacPresets

/* ============================================================
 * 温度风门策略接口
 * ============================================================ */

// 前向声明
struct DoorBuildResult;
struct DoorParams;
struct DoorPosition;
struct SealProfile;
struct BuildContext;

/**
 * @struct ParamDefinition
 * @brief 策略所需参数的定义(用于动态参数集注册)
 */
struct ParamDefinition {
    std::string id;         ///< 参数唯一ID
    std::string name;       ///< 显示名称
    std::string unit;       ///< 单位 "mm"/"deg"
    double minVal;          ///< 最小值
    double maxVal;          ///< 最大值
    double defaultVal;      ///< 默认值
    std::string description;///< 参数说明
};

/**
 * @class ITempDoorStrategy
 * @brief 温度风门构建策略接口
 * @details 所有温度风门类型(桶型/扇形/单片/蝶形/滑动)必须实现此接口。
 */
class ITempDoorStrategy {
public:
    virtual ~ITempDoorStrategy() = default;

    /** 策略名称 */
    virtual std::string getName() const = 0;

    /** 获取本策略需要的参数定义列表 */
    virtual std::vector<ParamDefinition> getRequiredParams() const = 0;

    /** 构建风门几何体 */
    virtual bool build(const std::map<std::string, double>& params,
                       BuildContext& ctx, tag_t& outBody) = 0;

    /** 运动学: 给定开度比(0.0=全关, 1.0=全开), 返回旋转角度 */
    virtual double solveAngleForRatio(double openRatio) const = 0;

    /** 计算指定开度时的有效通流面积 (mm²) */
    virtual double calcFlowArea(double openRatio) const = 0;

    /** 获取密封轮廓线坐标(供Module-4) */
    virtual std::vector<std::array<double, 3>> getSealContour() const = 0;

    /** 计算操作力矩 (N·mm) */
    virtual double calcTorque(double openRatio) const = 0;

    /** 在指定开度时与壳壁最小间隙 (mm) */
    virtual double checkMinClearance(double openRatio, tag_t shellBody) const = 0;
};

/**
 * @class IModeDoorStrategy
 * @brief 模式切换机构策略接口
 */
class IModeDoorStrategy {
public:
    virtual ~IModeDoorStrategy() = default;

    virtual std::string getName() const = 0;
    virtual std::vector<ParamDefinition> getRequiredParams() const = 0;

    /** 构建模式机构几何体 */
    virtual bool build(const std::map<std::string, double>& params,
                       BuildContext& ctx, tag_t& outBody) = 0;

    /** 获取指定模式时的机构位置角度 */
    virtual double getAngleForMode(AirMode mode) const = 0;

    /** 获取各出风口在指定模式时的开口面积比 (0~1) */
    virtual std::map<std::string, double> getOutletRatios(AirMode mode) const = 0;

    /** 获取密封轮廓 */
    virtual std::vector<std::array<double, 3>> getSealContour() const = 0;

    /** 计算操作力矩 */
    virtual double calcTorque(AirMode fromMode, AirMode toMode) const = 0;
};

/* ============================================================
 * 策略工厂
 * ============================================================ */

/**
 * @class DoorStrategyFactory
 * @brief 根据拓扑配置创建对应的风门构建策略
 */
class DoorStrategyFactory {
public:
    /** 创建温度风门策略实例 */
    static std::unique_ptr<ITempDoorStrategy> createTempDoorStrategy(TempDoorType type);

    /** 创建模式切换策略实例 */
    static std::unique_ptr<IModeDoorStrategy> createModeDoorStrategy(ModeDoorType type);
};

#endif /* HVAC_TOPOLOGY_CONFIG_H */
