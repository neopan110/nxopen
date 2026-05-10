/**
 * @file barrel_door_strategy.h
 * @brief 桶型风门(Barrel Door)构建策略
 * @details 实现ITempDoorStrategy接口。
 *
 * 【桶型风门几何本质】
 * 圆柱面的一段弧面(如同"桶壁的一部分")，绕圆柱轴旋转。
 * 旋转时弧面的不同区域对准/遮挡加热器通道，实现温度混合比调节。
 *
 * 【关键优势】(法雷奥专利体系, BBA标配)
 * 1. 风量线性度最佳: 开口面积 ∝ sin(θ)，接近线性
 * 2. 天然适配多温区: 轴向分段即可实现左右独立控制
 * 3. 密封性好: 弧面与壳壁圆弧配合，面密封而非线密封
 * 4. 低噪音: 旋转过渡平滑，无突变
 *
 * 【几何参数】
 * - barrelRadius: 桶型半径(圆柱面半径)
 * - barrelArcAngle: 弧面张角(圆柱面截取的角度范围)
 * - barrelLength: 轴向长度(决定风门宽度)
 * - barrelThickness: 壁厚
 * - rotationAxis: 旋转轴 = 圆柱中心线
 * - maxRotation: 最大旋转角度(从全冷到全热)
 *
 * 【构建步骤】
 * 1. 创建圆柱面(半径=barrelRadius, 长度=barrelLength)
 * 2. 裁剪为弧面(角度=barrelArcAngle, 如150°)
 * 3. 偏置为薄壳实体(厚度=barrelThickness)
 * 4. 两端加唇形密封凸缘
 * 5. 旋转轴处加轴孔/轴承座
 *
 * @version 2.0
 * @date 2026-05-10
 */

#ifndef HVAC_BARREL_DOOR_STRATEGY_H
#define HVAC_BARREL_DOOR_STRATEGY_H

#include "hvac_topology_config.h"
#include "hvac_common.h"

/* ============================================================
 * 桶型风门参数结构
 * ============================================================ */

/**
 * @struct BarrelDoorParams
 * @brief 单个桶型风门的完整参数集
 * @details 双温区时有Left和Right各一组
 */
struct BarrelDoorParams {
    // --- 基本几何 ---
    double barrelRadius = 65.0;     ///< 桶型半径 (mm), 范围40~100
    double barrelArcAngle = 150.0;  ///< 弧面张角 (deg), 范围120~200
    double barrelLength = 140.0;    ///< 轴向长度 (mm), 范围80~200
    double barrelThickness = 2.0;   ///< 壁厚 (mm), 范围1.5~2.5

    // --- 旋转轴位置 ---
    double axisCenterX = 200.0;     ///< 旋转轴中心X坐标 (mm)
    double axisCenterY = 0.0;       ///< 旋转轴中心Y坐标 (mm)
    double axisCenterZ = 180.0;     ///< 旋转轴中心Z坐标 (mm)

    // --- 运动范围 ---
    double maxRotation = 80.0;      ///< 最大旋转角度 (deg), 从全冷→全热
    double homeAngle = 0.0;         ///< 初始角度(全冷位) (deg)

    // --- 密封 ---
    double sealLipWidth = 3.0;      ///< 唇形密封凸缘宽度 (mm)
    double sealLipHeight = 2.0;     ///< 唇形密封凸缘高度 (mm)

    // --- 轴承 ---
    double shaftDiameter = 8.0;     ///< 轴直径 (mm)
    double bearingSeatWidth = 8.0;  ///< 轴承座宽度 (mm)
};

/* ============================================================
 * 桶型风门策略实现
 * ============================================================ */

class BarrelDoorStrategy : public ITempDoorStrategy {
public:
    BarrelDoorStrategy() = default;
    ~BarrelDoorStrategy() override = default;

    /* ---- ITempDoorStrategy接口实现 ---- */

    std::string getName() const override { return "BarrelDoor"; }

    std::vector<ParamDefinition> getRequiredParams() const override {
        return {
            {"barrelRadius",    "Barrel Radius",    "mm",  40.0, 100.0, 65.0,
             "Cylinder surface radius"},
            {"barrelArcAngle",  "Arc Angle",        "deg", 120.0, 200.0, 150.0,
             "Arc span angle of barrel surface"},
            {"barrelLength",    "Barrel Length",     "mm",  80.0, 200.0, 140.0,
             "Axial length (door width)"},
            {"barrelThickness", "Wall Thickness",   "mm",  1.5, 2.5, 2.0,
             "Barrel wall thickness"},
            {"axisCenterX",     "Axis Center X",    "mm",  100.0, 350.0, 200.0,
             "Rotation axis X position"},
            {"axisCenterZ",     "Axis Center Z",    "mm",  100.0, 280.0, 180.0,
             "Rotation axis Z position"},
            {"maxRotation",     "Max Rotation",     "deg", 60.0, 120.0, 80.0,
             "Full rotation range (cold to hot)"},
            {"sealLipWidth",    "Seal Lip Width",   "mm",  2.0, 5.0, 3.0,
             "Lip seal flange width"},
        };
    }

    /**
     * @brief 构建桶型风门几何体
     *
     * 【NX API构建流程】
     * 1. 创建圆柱体 (UF_MODL_create_cyl / NXOpen::Features::CylinderBuilder)
     * 2. 布尔交: 用扇形Block裁剪弧面范围
     * 3. 抽壳: 内偏置为薄壳
     * 4. 端部唇形密封: 拉伸小截面沿端部扫掠
     * 5. 轴孔: 布尔减去中心圆柱
     *
     * 【坑位】
     * - 圆柱面裁剪时注意角度基准(从X轴正方向逆时针)
     * - 薄壳偏置方向必须向内(向圆柱轴方向)
     */
    bool build(const std::map<std::string, double>& params,
               BuildContext& ctx, tag_t& outBody) override;

    /**
     * @brief 运动学求解
     * @details 桶型风门为纯旋转:
     *   angle = homeAngle + openRatio × maxRotation
     *   openRatio=0 → 全冷(冷风全通过)
     *   openRatio=1 → 全热(冷风全被遮挡,热风全通)
     */
    double solveAngleForRatio(double openRatio) const override;

    /**
     * @brief 计算有效通流面积
     * @details 桶型风门的通流面积:
     *   A(θ) = barrelLength × barrelRadius × (1 - cos(θ_effective))
     *   其中θ_effective = barrelArcAngle/2 - |旋转角 - barrelArcAngle/2|
     *
     * 比单片旋转(A∝cosθ)线性度好得多
     */
    double calcFlowArea(double openRatio) const override;

    /** 密封轮廓: 弧面两端 + 两侧唇形 (4条密封线) */
    std::vector<std::array<double, 3>> getSealContour() const override;

    /**
     * @brief 操作力矩
     * @details 桶型风门力矩来源:
     *   1. 密封摩擦力矩 = 密封压缩力 × 唇形接触长度 × 摩擦系数 × 半径
     *   2. 气动力矩 = 压差 × 弧面投影面积 × 力臂
     *   3. 惯性力矩(低速可忽略)
     *
     *   M_total = M_seal + M_aero
     *   设计准则: M_total ≤ 执行器额定 × 0.7 (安全系数)
     */
    double calcTorque(double openRatio) const override;

    /** 间隙检查: 弧面外表面与壳壁内表面的最小距离 */
    double checkMinClearance(double openRatio, tag_t shellBody) const override;

    /* ---- 桶型专用方法 ---- */

    /** 设置参数(从map解析) */
    void setParams(const std::map<std::string, double>& params);

    /** 获取当前参数 */
    const BarrelDoorParams& getBarrelParams() const { return m_params; }

    /** 获取指定开度时弧面上各点坐标(用于干涉分析) */
    std::vector<Vec3d> getArcPointsAtAngle(double angle, int numPoints = 36) const;

private:
    BarrelDoorParams m_params;
    tag_t m_bodyTag = NULL_TAG;

    /** 内部: 创建圆柱面弧段实体 */
    tag_t createBarrelBody(BuildContext& ctx);

    /** 内部: 添加唇形密封特征 */
    void addSealLips(tag_t bodyTag, BuildContext& ctx);

    /** 内部: 创建轴孔 */
    void createShaftHole(tag_t bodyTag, BuildContext& ctx);
};

#endif /* HVAC_BARREL_DOOR_STRATEGY_H */
