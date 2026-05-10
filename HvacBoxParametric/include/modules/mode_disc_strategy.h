/**
 * @file mode_disc_strategy.h
 * @brief 模式盘(Mode Disc)构建策略
 * @details 实现IModeDoorStrategy接口。
 *
 * 【模式盘几何本质】
 * 一个圆盘(或环形盘)，盘面上开有多个窗口(开口)。
 * 盘旋转不同角度时，不同窗口对准不同的出风通道入口，
 * 实现DEF/FACE/FOOT等模式切换。
 *
 * 【核心优势】
 * 1. 一个执行器控制所有模式 → 成本低、可靠性高
 * 2. 结构紧凑 → 节省箱体内部空间
 * 3. 模式切换连续 → 可实现中间过渡状态(BI-LEVEL)
 * 4. 直连执行器 → 无需四连杆(零间隙传动)
 *
 * 【模式映射】(盘旋转角度 → 出风模式)
 *   0°:   FACE (Face窗口对齐Face通道)
 *   25°:  BI-LEVEL (Face+Foot窗口同时部分对齐)
 *   50°:  FOOT
 *   75°:  FOOT+DEF
 *   100°: DEF (Def窗口对齐Def通道)
 *
 * 【窗口设计准则】
 * - 窗口形状: 扇形环(环形盘) 或 扇形(实心盘)
 * - 窗口面积: 与对应出风口截面积匹配(避免节流)
 * - 窗口间隔: 相邻窗口间的"实体区"= 密封面
 * - 盘面密封: 盘面与壳体之间的平面接触(面密封)
 *
 * @version 2.0
 * @date 2026-05-10
 */

#ifndef HVAC_MODE_DISC_STRATEGY_H
#define HVAC_MODE_DISC_STRATEGY_H

#include "hvac_topology_config.h"
#include "hvac_common.h"

/* ============================================================
 * 模式盘参数结构
 * ============================================================ */

struct ModeDiscParams {
    // --- 盘基本几何 ---
    double discOuterRadius = 85.0;  ///< 盘外径 (mm), 范围60~120
    double discInnerRadius = 25.0;  ///< 盘内径 (mm, 0=实心盘, >0=环形盘)
    double discThickness = 4.0;     ///< 盘厚 (mm), 范围3~6
    double discCenterX = 350.0;     ///< 盘中心X坐标 (mm)
    double discCenterY = 0.0;       ///< 盘中心Y坐标 (mm)
    double discCenterZ = 200.0;     ///< 盘中心Z坐标 (mm)

    // --- 旋转 ---
    double rotationRange = 120.0;   ///< 总旋转范围 (deg)
    double homeAngle = 0.0;         ///< 初始角(FACE模式) (deg)

    // --- 窗口参数 ---
    // DEF窗口
    double defWindowStart = 10.0;   ///< DEF窗口起始角 (deg)
    double defWindowSpan = 35.0;    ///< DEF窗口张角 (deg)
    double defWindowInnerR = 30.0;  ///< DEF窗口内径 (mm)
    double defWindowOuterR = 75.0;  ///< DEF窗口外径 (mm)

    // FACE窗口
    double faceWindowStart = 55.0;
    double faceWindowSpan = 40.0;
    double faceWindowInnerR = 30.0;
    double faceWindowOuterR = 80.0;

    // FOOT窗口
    double footWindowStart = 105.0;
    double footWindowSpan = 38.0;
    double footWindowInnerR = 28.0;
    double footWindowOuterR = 78.0;

    // --- 密封 ---
    double sealGrooveWidth = 2.5;   ///< 盘面密封槽宽度 (mm)
    double sealGrooveDepth = 1.5;   ///< 盘面密封槽深度 (mm)

    // --- 轴 ---
    double shaftDiameter = 10.0;    ///< 驱动轴直径 (mm)

    // --- 模式角度映射 ---
    double angleFace = 0.0;         ///< FACE模式时盘角度 (deg)
    double angleBiLevel = 25.0;     ///< BI-LEVEL模式角度 (deg)
    double angleFoot = 50.0;        ///< FOOT模式角度 (deg)
    double angleFootDef = 75.0;     ///< FOOT+DEF模式角度 (deg)
    double angleDef = 100.0;        ///< DEF模式角度 (deg)
};

/* ============================================================
 * 模式盘策略实现
 * ============================================================ */

class ModeDiscStrategy : public IModeDoorStrategy {
public:
    ModeDiscStrategy() = default;
    ~ModeDiscStrategy() override = default;

    /* ---- IModeDoorStrategy接口实现 ---- */

    std::string getName() const override { return "ModeDisc"; }

    std::vector<ParamDefinition> getRequiredParams() const override {
        return {
            {"discOuterRadius",   "Disc Outer Radius",  "mm",  60.0, 120.0, 85.0,
             "Mode disc outer diameter"},
            {"discInnerRadius",   "Disc Inner Radius",  "mm",  0.0, 50.0, 25.0,
             "Inner radius (0=solid disc)"},
            {"discThickness",     "Disc Thickness",     "mm",  3.0, 6.0, 4.0,
             "Disc plate thickness"},
            {"discCenterX",       "Disc Center X",      "mm",  200.0, 450.0, 350.0,
             "Disc rotation center X"},
            {"discCenterZ",       "Disc Center Z",      "mm",  100.0, 280.0, 200.0,
             "Disc rotation center Z"},
            {"rotationRange",     "Rotation Range",     "deg", 80.0, 150.0, 120.0,
             "Total disc rotation range"},
            {"defWindowSpan",     "DEF Window Span",    "deg", 20.0, 60.0, 35.0,
             "Defrost window angular span"},
            {"faceWindowSpan",    "FACE Window Span",   "deg", 25.0, 60.0, 40.0,
             "Face vent window angular span"},
            {"footWindowSpan",    "FOOT Window Span",   "deg", 25.0, 55.0, 38.0,
             "Foot vent window angular span"},
        };
    }

    /**
     * @brief 构建模式盘几何体
     *
     * 【NX API构建流程】
     * 1. 创建圆柱体(外径, 厚度) → 盘基体
     * 2. 如果内径>0: 布尔减去内圆柱 → 环形盘
     * 3. 布尔减去DEF窗口(扇形环拉伸体)
     * 4. 布尔减去FACE窗口
     * 5. 布尔减去FOOT窗口
     * 6. 中心轴孔
     * 7. 盘面密封槽(环形槽)
     * 8. 边缘倒角(模具脱模)
     *
     * 【窗口创建方法】
     * 窗口 = 扇形环(AnnularSector)拉伸后布尔减:
     *   在XZ平面绘制扇形环截面 → 沿Y方向拉伸discThickness → Subtract
     */
    bool build(const std::map<std::string, double>& params,
               BuildContext& ctx, tag_t& outBody) override;

    /**
     * @brief 获取指定模式的盘旋转角度
     * @details 直连执行器，角度直接对应模式:
     *   FACE → 0°, BI_LEVEL → 25°, FOOT → 50°,
     *   FOOT_DEF → 75°, DEF → 100°
     */
    double getAngleForMode(AirMode mode) const override;

    /**
     * @brief 各出风口在指定模式时的开口面积比
     * @details 通过计算窗口与通道的重叠面积得到:
     *   FACE模式: DEF=0%, FACE=100%, FOOT=0%
     *   BI_LEVEL: DEF=0%, FACE=60%, FOOT=40%
     *   FOOT:    DEF=0%, FACE=0%, FOOT=100%
     *   FOOT_DEF: DEF=50%, FACE=0%, FOOT=50%
     *   DEF:     DEF=100%, FACE=0%, FOOT=0%
     */
    std::map<std::string, double> getOutletRatios(AirMode mode) const override;

    /** 密封轮廓: 盘面周向 + 各窗口边缘 */
    std::vector<std::array<double, 3>> getSealContour() const override;

    /**
     * @brief 模式切换力矩
     * @details 模式盘力矩较小(面密封摩擦为主):
     *   M = μ × F_seal × R_mean
     *   μ = 0.3~0.5 (橡胶/PP摩擦系数)
     *   F_seal = 密封条压缩力 × 接触面积
     *   R_mean = (内径+外径)/2
     */
    double calcTorque(AirMode fromMode, AirMode toMode) const override;

    /* ---- 模式盘专用方法 ---- */

    /** 设置参数 */
    void setParams(const std::map<std::string, double>& params);

    /** 获取参数 */
    const ModeDiscParams& getDiscParams() const { return m_params; }

    /** 获取所有模式位置定义 */
    std::vector<ModePosition> getModePositions() const;

    /** 检查窗口间是否有足够密封面(最小5°间隔) */
    bool validateWindowSpacing() const;

    /** 获取盘面上各窗口的2D轮廓(用于出图标注) */
    struct WindowContour {
        std::string name;
        std::vector<std::array<double, 2>> points; // (R, θ) 极坐标
    };
    std::vector<WindowContour> getWindowContours() const;

private:
    ModeDiscParams m_params;
    tag_t m_bodyTag = NULL_TAG;

    /** 创建扇形环拉伸体(用于窗口布尔减) */
    tag_t createAnnularSectorBody(double innerR, double outerR,
                                   double startAngle, double spanAngle,
                                   double thickness, BuildContext& ctx);

    /** 创建盘基体 */
    tag_t createDiscBase(BuildContext& ctx);

    /** 开窗(布尔减) */
    void cutWindows(tag_t discBody, BuildContext& ctx);

    /** 添加密封槽 */
    void addSealGrooves(tag_t discBody, BuildContext& ctx);
};

#endif /* HVAC_MODE_DISC_STRATEGY_H */
