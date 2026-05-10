/**
 * @file hvac_parameters.h
 * @brief HVAC箱体参数化系统 - 参数定义与管理
 * @details 定义Level-0/1/2全部参数的数据结构、范围约束、
 *          序列化/反序列化接口。所有参数通过HvacParameterSet统一管理。
 *
 * 【设计原则】
 * 1. Level-0: 整车输入参数, 由外部Package数据确定, 程序内不可修改
 * 2. Level-1: 驱动参数, 由设计师在对话框中设定, 触发全模型联动
 * 3. Level-2: 关联参数, 由Level-1通过公式自动计算, 设计师不直接编辑
 *
 * @version 1.0
 * @date 2026-05-10
 */

#ifndef HVAC_PARAMETERS_H
#define HVAC_PARAMETERS_H

#include "hvac_common.h"

/* ============================================================
 * Level-0: 整车输入参数 (外部约束)
 * ============================================================ */

/**
 * @struct HvacLevel0Params
 * @brief 整车Package输入参数集
 * @details 来源于OEM整车Package数据、热系统规格书、性能目标。
 *          这些参数在设计启动时确定, 箱体设计过程中不可变更。
 */
struct HvacLevel0Params {
    // --- 空间包络 ---
    double envelopeX = 420.0;       ///< L0-001 仪表板下方可用空间X (mm), 范围350-500
    double envelopeY = 320.0;       ///< L0-001 仪表板下方可用空间Y (mm), 范围280-400
    double envelopeZ = 260.0;       ///< L0-001 仪表板下方可用空间Z (mm), 范围200-320

    // --- 蒸发器芯体 ---
    double evapWidth = 240.0;       ///< L0-002 蒸发器宽度W (mm), 范围200-280
    double evapHeight = 210.0;      ///< L0-002 蒸发器高度H (mm), 范围180-240
    double evapDepth = 48.0;        ///< L0-002 蒸发器深度D (mm), 范围38-58

    // --- 加热器芯体 ---
    double heaterWidth = 210.0;     ///< L0-003 加热器宽度W (mm), 范围180-250
    double heaterHeight = 175.0;    ///< L0-003 加热器高度H (mm), 范围150-200
    double heaterDepth = 32.0;      ///< L0-003 加热器深度D (mm), 范围26-42

    // --- 鼓风机 ---
    double blowerDiameter = 160.0;  ///< L0-004 鼓风机蜗壳外径 (mm), 范围140-180

    // --- 性能目标 ---
    double coolingCapacity = 5.0;   ///< L0-005 制冷量目标 (kW), 范围3.5-7.0
    double heatingCapacity = 6.0;   ///< L0-006 制热量目标 (kW), 范围4.0-8.0
    double maxAirflow = 500.0;      ///< L0-007 最大风量 (m³/h), 范围400-600

    // --- 安装硬点 ---
    Vec3d mountOrigin = {0.0, 0.0, 0.0}; ///< L0-008 安装硬点坐标系原点

    // --- 配置选项 ---
    bool hasRearOutlet = false;     ///< L0-009 是否有后排出风口
    DriveSide driveSide = DriveSide::LHD; ///< L0-010 左右驾定义
};

/* ============================================================
 * Level-1: 驱动参数 (设计师设定)
 * ============================================================ */

/**
 * @struct HvacLevel1Params
 * @brief 箱体核心驱动参数集
 * @details 设计师在对话框中主动设定的参数, 变更后触发全模型联动重建。
 *          每个参数都有明确的允许范围, 超出则拒绝赋值。
 */
struct HvacLevel1Params {
    // --- 箱体总体尺寸 ---
    double boxLengthX = 420.0;      ///< L1-001 箱体总长度X方向 (mm), 350-500
    double boxWidthY = 320.0;       ///< L1-002 箱体总宽度Y方向 (mm), 280-400
    double boxHeightZ = 260.0;      ///< L1-003 箱体总高度Z方向 (mm), 200-320

    // --- 芯体安装参数 ---
    double evapTiltAngle = 5.0;     ///< L1-004 蒸发器安装倾角 (deg), 0-15
    double heaterTiltAngle = 55.0;  ///< L1-005 加热器安装倾角 (deg), 30-75
    double coreSpacing = 90.0;      ///< L1-006 蒸发器至加热器中心距 (mm), 60-120

    // --- 模具/壳体参数 ---
    double partingPlaneZ = 130.0;   ///< L1-007 主分型面Z坐标 (mm), 箱体高度40%-60%
    double wallThickness = 2.5;     ///< L1-008 壳体公称壁厚 (mm), 2.0-3.5
    double sealGrooveWidth = 4.0;   ///< L1-009 密封槽宽度 (mm), 3.0-5.0

    // --- 风门参数 ---
    double doorAxisToWall = 22.0;   ///< L1-010 风门轴线至箱体侧壁距离 (mm), 15-35

    // --- 出风口位置 ---
    double defOutletZ = 230.0;      ///< L1-011 DEF出风口中心高度 (mm), 顶面下30-80
    double faceOutletZ = 170.0;     ///< L1-012 FACE出风口中心高度 (mm), 箱体中部
    double footOutletZ = 40.0;      ///< L1-013 FOOT出风口中心高度 (mm), 底部上20-60

    // --- 温度风门旋转中心 ---
    double tempDoorCenterX = 200.0; ///< L1-014 温度风门旋转中心X坐标 (mm)
    double tempDoorCenterZ = 180.0; ///< L1-015 温度风门旋转中心Z坐标 (mm)

    // --- 模式风门旋转中心 ---
    double modeDoorCenterX = 350.0; ///< L1-016 模式风门旋转中心X (mm)
    double modeDoorCenterZ = 200.0; ///< L1-016 模式风门旋转中心Z (mm)

    // --- 执行器 ---
    double actuatorOffsetY = 15.0;  ///< L1-017 执行器安装面Y坐标偏移 (mm), 5-25
};

/* ============================================================
 * Level-2: 关联参数 (自动计算)
 * ============================================================ */

/**
 * @struct HvacLevel2Params
 * @brief 关联派生参数集
 * @details 由Level-1参数通过确定性公式自动计算, 设计师不直接编辑。
 *          每次Level-1变更后由ParameterEngine重新计算。
 */
struct HvacLevel2Params {
    // --- 腔室尺寸 ---
    double evapChamberW = 0.0;      ///< L2-001 蒸发器腔室内廓宽度 (mm)
    double evapChamberH = 0.0;      ///< L2-001 蒸发器腔室内廓高度 (mm)
    double evapChamberD = 0.0;      ///< L2-001 蒸发器腔室内廓深度 (mm)
    double heaterChamberW = 0.0;    ///< L2-002 加热器腔室内廓宽度 (mm)
    double heaterChamberH = 0.0;    ///< L2-002 加热器腔室内廓高度 (mm)
    double heaterChamberD = 0.0;    ///< L2-002 加热器腔室内廓深度 (mm)

    // --- 温度风门 ---
    double tempDoorRadius = 0.0;    ///< L2-003 温度风门旋转半径 (mm)
    double tempDoorSweepAngle = 0.0;///< L2-003 温度风门摆动角度 (deg)
    double tempDoorArcLength = 0.0; ///< L2-003 温度风门有效弧长 (mm)
    double tempDoorShaftLength = 0.0;///< L2-004 温度风门轴长度 (mm)

    // --- 连杆机构 ---
    Vec3d linkagePivotP1 = {};      ///< L2-005 连杆铰接点P1坐标
    Vec3d linkagePivotP2 = {};      ///< L2-005 连杆铰接点P2坐标
    double couplerLength = 0.0;     ///< L2-010 执行器连杆长度 (mm)
    double crankLength = 0.0;       ///< 曲柄长度 (mm)
    double rockerLength = 0.0;      ///< 摇杆长度 (mm)

    // --- 密封/卡扣 ---
    double sealTotalLength = 0.0;   ///< L2-006 密封面总长度 (mm)
    std::vector<Vec3d> snapFitPositions; ///< L2-007 卡扣分布坐标

    // --- 拔模/加强筋 ---
    Vec3d draftDirection = {0, 0, 1}; ///< L2-008 主拔模方向向量
    double ribHeight = 0.0;         ///< L2-009 加强筋高度 (mm)
    double ribSpacing = 0.0;        ///< L2-009 加强筋间距 (mm)
};

/* ============================================================
 * 参数范围注册表
 * ============================================================ */

/**
 * @class HvacParamRangeRegistry
 * @brief 参数范围查询注册表
 * @details 集中管理所有Level-1参数的允许范围, 用于对话框输入校验和联动前检查。
 */
class HvacParamRangeRegistry {
public:
    /** 获取单例实例 */
    static HvacParamRangeRegistry& instance() {
        static HvacParamRangeRegistry inst;
        return inst;
    }

    /** 查询指定参数的范围 */
    ParamRange getRange(const std::string& paramId) const {
        auto it = m_ranges.find(paramId);
        if (it != m_ranges.end()) return it->second;
        return {}; // 未注册的参数返回空范围
    }

    /** 校验参数值状态 */
    ParamStatus validate(const std::string& paramId, double value) const {
        auto it = m_ranges.find(paramId);
        if (it == m_ranges.end()) return ParamStatus::VALID; // 未注册跳过

        const ParamRange& range = it->second;
        if (!range.isInRange(value)) return ParamStatus::FAIL_OUT_RANGE;
        if (range.isNearLimit(value)) return ParamStatus::WARN_NEAR_LIMIT;
        return ParamStatus::VALID;
    }

private:
    HvacParamRangeRegistry() { registerAllRanges(); }
    HvacParamRangeRegistry(const HvacParamRangeRegistry&) = delete;
    HvacParamRangeRegistry& operator=(const HvacParamRangeRegistry&) = delete;

    void registerAllRanges() {
        // Level-1 驱动参数范围注册
        m_ranges["L1_001_boxLengthX"]     = {350.0, 500.0, 420.0, "mm"};
        m_ranges["L1_002_boxWidthY"]      = {280.0, 400.0, 320.0, "mm"};
        m_ranges["L1_003_boxHeightZ"]     = {200.0, 320.0, 260.0, "mm"};
        m_ranges["L1_004_evapTiltAngle"]  = {0.0, 15.0, 5.0, "deg"};
        m_ranges["L1_005_heaterTiltAngle"]= {30.0, 75.0, 55.0, "deg"};
        m_ranges["L1_006_coreSpacing"]    = {60.0, 120.0, 90.0, "mm"};
        m_ranges["L1_007_partingPlaneZ"]  = {80.0, 192.0, 130.0, "mm"}; // 40%~60% of 200~320
        m_ranges["L1_008_wallThickness"]  = {2.0, 3.5, 2.5, "mm"};
        m_ranges["L1_009_sealGrooveWidth"]= {3.0, 5.0, 4.0, "mm"};
        m_ranges["L1_010_doorAxisToWall"] = {15.0, 35.0, 22.0, "mm"};
        m_ranges["L1_011_defOutletZ"]     = {180.0, 290.0, 230.0, "mm"}; // 顶面下30-80
        m_ranges["L1_012_faceOutletZ"]    = {100.0, 220.0, 170.0, "mm"};
        m_ranges["L1_013_footOutletZ"]    = {20.0, 60.0, 40.0, "mm"};
        m_ranges["L1_014_tempDoorCenterX"]= {100.0, 350.0, 200.0, "mm"};
        m_ranges["L1_015_tempDoorCenterZ"]= {100.0, 280.0, 180.0, "mm"};
        m_ranges["L1_016_modeDoorCenterX"]= {200.0, 450.0, 350.0, "mm"};
        m_ranges["L1_017_actuatorOffsetY"]= {5.0, 25.0, 15.0, "mm"};
    }

    std::map<std::string, ParamRange> m_ranges;
};

/* ============================================================
 * 参数集合管理器
 * ============================================================ */

/**
 * @class HvacParameterSet
 * @brief 完整参数集合 (L0 + L1 + L2)
 * @details 统一管理三级参数, 提供加载/保存/校验/导出NX Expression接口。
 */
class HvacParameterSet {
public:
    HvacParameterSet() = default;
    ~HvacParameterSet() = default;

    // --- 参数访问 ---
    HvacLevel0Params& level0() { return m_level0; }
    const HvacLevel0Params& level0() const { return m_level0; }

    HvacLevel1Params& level1() { return m_level1; }
    const HvacLevel1Params& level1() const { return m_level1; }

    HvacLevel2Params& level2() { return m_level2; }
    const HvacLevel2Params& level2() const { return m_level2; }

    // --- Level-1参数校验 ---

    /**
     * @brief 校验所有Level-1参数
     * @return 校验失败的参数列表 (参数ID, 状态)
     */
    std::vector<std::pair<std::string, ParamStatus>> validateLevel1() const;

    /**
     * @brief 校验单个参数
     * @param paramId 参数标识符
     * @param value 待校验值
     * @return 参数状态
     */
    ParamStatus validateSingleParam(const std::string& paramId, double value) const;

    // --- Level-2自动计算 ---

    /**
     * @brief 根据当前Level-0/1参数重新计算所有Level-2参数
     * @details 调用时机: 任何Level-1参数变更后
     */
    void recalculateLevel2();

    // --- JSON序列化 ---

    /**
     * @brief 从JSON文件加载参数(L0+L1默认值)
     * @param filePath JSON配置文件路径
     * @return true=成功, false=失败(文件不存在或格式错误)
     */
    bool loadFromJson(const std::string& filePath);

    /**
     * @brief 保存当前参数到JSON文件
     * @param filePath 输出文件路径
     * @return true=成功
     */
    bool saveToJson(const std::string& filePath) const;

    // --- NX Expression同步 ---

    /**
     * @brief 将所有Level-1参数写入NX Part的Expression
     * @param partTag 目标Part的tag
     * @details Expression命名规则: "HVAC_L1_参数名"
     *          如 "HVAC_L1_boxLengthX = 420"
     */
    void exportToNxExpressions(tag_t partTag) const;

    /**
     * @brief 从NX Part的Expression读取Level-1参数
     * @param partTag 源Part的tag
     */
    void importFromNxExpressions(tag_t partTag);

private:
    HvacLevel0Params m_level0;
    HvacLevel1Params m_level1;
    HvacLevel2Params m_level2;

    // --- Level-2计算子函数 ---
    void calcChamberSizes();        ///< 计算腔室尺寸
    void calcTempDoorGeometry();    ///< 计算温度风门几何
    void calcLinkageKinematics();   ///< 计算连杆运动学
    void calcSealAndSnaps();        ///< 计算密封面与卡扣
    void calcRibParameters();       ///< 计算加强筋参数
};

#endif /* HVAC_PARAMETERS_H */
