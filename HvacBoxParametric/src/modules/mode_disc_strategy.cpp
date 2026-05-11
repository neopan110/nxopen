/**
 * @file mode_disc_strategy.cpp
 * @brief 模式盘策略实现
 * @version 2.0
 * @date 2026-05-10
 */

#include "modules/mode_disc_strategy.h"
#include "utils/error_handler.h"
#include <cmath>

/* ============================================================
 * 参数解析
 * ============================================================ */

void ModeDiscStrategy::setParams(const std::map<std::string, double>& params)
{
    auto get = [&](const std::string& key, double defaultVal) -> double {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : defaultVal;
    };

    m_params.discOuterRadius  = get("discOuterRadius", 85.0);
    m_params.discInnerRadius  = get("discInnerRadius", 25.0);
    m_params.discThickness    = get("discThickness", 4.0);
    m_params.discCenterX      = get("discCenterX", 350.0);
    m_params.discCenterY      = get("discCenterY", 0.0);
    m_params.discCenterZ      = get("discCenterZ", 200.0);
    m_params.rotationRange    = get("rotationRange", 120.0);
    m_params.defWindowSpan    = get("defWindowSpan", 35.0);
    m_params.faceWindowSpan   = get("faceWindowSpan", 40.0);
    m_params.footWindowSpan   = get("footWindowSpan", 38.0);
    m_params.shaftDiameter    = get("shaftDiameter", 10.0);
}

/* ============================================================
 * 几何构建
 * ============================================================ */

bool ModeDiscStrategy::build(const std::map<std::string, double>& params,
                             BuildContext& ctx, tag_t& outBody)
{
    setParams(params);
    tag_t partTag = ctx.workPartTag;
    if (partTag == NULL_TAG) return false;

    HvacErrorHandler::logInfo("  ModeDisc: Building disc R=" +
                              std::to_string(m_params.discOuterRadius) +
                              " Thk=" + std::to_string(m_params.discThickness));

    try {
        // Step-1: 创建盘基体
        tag_t discBody = createDiscBase(ctx);
        if (discBody == NULL_TAG) {
            HvacErrorHandler::logError("ModeDisc: Failed to create disc base");
            return false;
        }

        // Step-2: 开窗
        cutWindows(discBody, ctx);

        // Step-3: 密封槽
        addSealGrooves(discBody, ctx);

        // Step-4: 中心轴孔
        double holeOrigin[3] = {
            m_params.discCenterX,
            m_params.discCenterY - m_params.discThickness / 2.0,
            m_params.discCenterZ
        };
        char radStr[32], htStr[32];
        snprintf(radStr, sizeof(radStr), "%.4f", m_params.shaftDiameter / 2.0);
        snprintf(htStr, sizeof(htStr), "%.4f", m_params.discThickness * 2.0);

        tag_t holeFeat = NULL_TAG;
        UF_MODL_create_cyl1(UF_NULLSIGN, holeOrigin, htStr, radStr, &holeFeat);
        if (holeFeat != NULL_TAG) {
            tag_t holeBody = NULL_TAG;
            UF_MODL_ask_feat_body(holeFeat, &holeBody);
            if (holeBody != NULL_TAG) {
                tag_t resultBody = NULL_TAG;
                UF_MODL_boolean(discBody, holeBody, 2, &resultBody);
                if (resultBody != NULL_TAG) discBody = resultBody;
            }
        }

        UF_MODL_update();

        m_bodyTag = discBody;
        outBody = discBody;

        HvacErrorHandler::logInfo("  ModeDisc: Build complete. Tag=" +
                                  std::to_string(discBody));
        return true;

    } catch (const std::exception& ex) {
        HvacErrorHandler::logError("ModeDisc build exception: " +
                                    std::string(ex.what()));
        return false;
    }
}

tag_t ModeDiscStrategy::createDiscBase(BuildContext& ctx)
{
    // 创建外圆柱(盘基体)
    // 盘面法线沿Y方向(盘面朝向气流方向)
    double origin[3] = {
        m_params.discCenterX,
        m_params.discCenterY - m_params.discThickness / 2.0,
        m_params.discCenterZ
    };

    char radStr[32], htStr[32];
    snprintf(radStr, sizeof(radStr), "%.4f", m_params.discOuterRadius);
    snprintf(htStr, sizeof(htStr), "%.4f", m_params.discThickness);

    tag_t outerFeat = NULL_TAG;
    int rc = UF_MODL_create_cyl1(UF_NULLSIGN, origin, htStr, radStr, &outerFeat);
    if (rc != 0 || outerFeat == NULL_TAG) return NULL_TAG;

    tag_t discBody = NULL_TAG;
    UF_MODL_ask_feat_body(outerFeat, &discBody);

    // 如果是环形盘(内径>0): 减去内圆柱
    if (m_params.discInnerRadius > 1.0) {
        char innerRadStr[32];
        snprintf(innerRadStr, sizeof(innerRadStr), "%.4f", m_params.discInnerRadius);

        tag_t innerFeat = NULL_TAG;
        rc = UF_MODL_create_cyl1(UF_NULLSIGN, origin, htStr, innerRadStr, &innerFeat);
        if (rc == 0 && innerFeat != NULL_TAG) {
            tag_t innerBody = NULL_TAG;
            UF_MODL_ask_feat_body(innerFeat, &innerBody);
            if (innerBody != NULL_TAG) {
                tag_t resultBody = NULL_TAG;
                UF_MODL_boolean(discBody, innerBody, 2 /*SUBTRACT*/, &resultBody);
                if (resultBody != NULL_TAG) discBody = resultBody;
            }
        }
    }

    return discBody;
}

void ModeDiscStrategy::cutWindows(tag_t discBody, BuildContext& ctx)
{
    /**
     * 窗口开孔: 使用扇形环拉伸体布尔减
     *
     * 每个窗口定义: (startAngle, spanAngle, innerR, outerR)
     * 创建方法: 用Block近似扇形(Phase-2用精确Revolve)
     *
     * 简化方案: 用圆柱段(较小半径)模拟窗口
     */

    struct WindowDef {
        std::string name;
        double startAngle;
        double spanAngle;
        double innerR;
        double outerR;
    };

    std::vector<WindowDef> windows = {
        {"DEF",  m_params.defWindowStart,  m_params.defWindowSpan,
                 m_params.defWindowInnerR, m_params.defWindowOuterR},
        {"FACE", m_params.faceWindowStart, m_params.faceWindowSpan,
                 m_params.faceWindowInnerR, m_params.faceWindowOuterR},
        {"FOOT", m_params.footWindowStart, m_params.footWindowSpan,
                 m_params.footWindowInnerR, m_params.footWindowOuterR},
    };

    for (const auto& win : windows) {
        tag_t windowBody = createAnnularSectorBody(
            win.innerR, win.outerR,
            win.startAngle, win.spanAngle,
            m_params.discThickness * 2.0, // 确保贯穿
            ctx);

        if (windowBody != NULL_TAG) {
            tag_t resultBody = NULL_TAG;
            int rc = UF_MODL_boolean(discBody, windowBody, 2 /*SUBTRACT*/, &resultBody);
            if (rc == 0 && resultBody != NULL_TAG) {
                discBody = resultBody;
            }
            HvacErrorHandler::logInfo("    Window [" + win.name + "] cut at " +
                                      std::to_string(win.startAngle) + " deg");
        }
    }
}

tag_t ModeDiscStrategy::createAnnularSectorBody(double innerR, double outerR,
                                                 double startAngle, double spanAngle,
                                                 double thickness, BuildContext& ctx)
{
    /**
     * 扇形环实体创建 (简化方案):
     *
     * 精确方案: 在XZ平面绘制扇形环Sketch → Extrude沿Y → 定位到盘中心
     * 简化方案: 用一个Box近似扇形区域(Phase-1/2过渡)
     *
     * Phase-3: 使用Revolve或Sketch精确创建
     */

    // 简化: 创建一个定位到窗口中心角的Box
    double midAngle = degToRad(startAngle + spanAngle / 2.0);
    double midR = (innerR + outerR) / 2.0;
    double radialWidth = outerR - innerR;
    double arcWidth = midR * degToRad(spanAngle); // 弧长近似

    // Box中心位置(相对于盘中心)
    double boxCenterX = m_params.discCenterX + midR * std::cos(midAngle);
    double boxCenterZ = m_params.discCenterZ + midR * std::sin(midAngle);

    double origin[3] = {
        boxCenterX - radialWidth / 2.0,
        m_params.discCenterY - thickness / 2.0,
        boxCenterZ - arcWidth / 2.0
    };

    char sx[32], sy[32], sz[32];
    snprintf(sx, sizeof(sx), "%.4f", radialWidth);
    snprintf(sy, sizeof(sy), "%.4f", thickness);
    snprintf(sz, sizeof(sz), "%.4f", arcWidth);
    char* edgeLens[3] = {sx, sy, sz};

    tag_t blockFeat = NULL_TAG;
    UF_MODL_create_block(UF_NULLSIGN, NULL_TAG, origin, edgeLens, &blockFeat);

    if (blockFeat == NULL_TAG) return NULL_TAG;

    tag_t blockBody = NULL_TAG;
    UF_MODL_ask_feat_body(blockFeat, &blockBody);
    return blockBody;
}

void ModeDiscStrategy::addSealGrooves(tag_t discBody, BuildContext& ctx)
{
    // Phase-2: 在盘面添加环形密封槽
    // 简化: 跳过(密封由壳体侧实现)
    HvacErrorHandler::logInfo("    Seal grooves: deferred to Phase-4 (Seal module)");
}

/* ============================================================
 * 模式角度映射
 * ============================================================ */

double ModeDiscStrategy::getAngleForMode(AirMode mode) const
{
    switch (mode) {
        case AirMode::FACE:     return m_params.angleFace;
        case AirMode::BI_LEVEL: return m_params.angleBiLevel;
        case AirMode::FOOT:     return m_params.angleFoot;
        case AirMode::FOOT_DEF: return m_params.angleFootDef;
        case AirMode::DEF:      return m_params.angleDef;
        default:                return 0.0;
    }
}

std::map<std::string, double> ModeDiscStrategy::getOutletRatios(AirMode mode) const
{
    std::map<std::string, double> ratios;

    switch (mode) {
        case AirMode::FACE:
            ratios["DEF"] = 0.0;  ratios["FACE"] = 1.0;  ratios["FOOT"] = 0.0;
            break;
        case AirMode::BI_LEVEL:
            ratios["DEF"] = 0.0;  ratios["FACE"] = 0.6;  ratios["FOOT"] = 0.4;
            break;
        case AirMode::FOOT:
            ratios["DEF"] = 0.0;  ratios["FACE"] = 0.0;  ratios["FOOT"] = 1.0;
            break;
        case AirMode::FOOT_DEF:
            ratios["DEF"] = 0.5;  ratios["FACE"] = 0.0;  ratios["FOOT"] = 0.5;
            break;
        case AirMode::DEF:
            ratios["DEF"] = 1.0;  ratios["FACE"] = 0.0;  ratios["FOOT"] = 0.0;
            break;
        default:
            ratios["DEF"] = 0.0;  ratios["FACE"] = 0.0;  ratios["FOOT"] = 0.0;
            break;
    }

    return ratios;
}

/* ============================================================
 * 密封轮廓
 * ============================================================ */

std::vector<std::array<double, 3>> ModeDiscStrategy::getSealContour() const
{
    std::vector<std::array<double, 3>> contour;

    // 盘面外圈密封(圆形)
    int points = 72;
    for (int i = 0; i < points; ++i) {
        double angle = 2.0 * HvacConst::HVAC_PI * i / points;
        contour.push_back({
            m_params.discCenterX + m_params.discOuterRadius * std::cos(angle),
            m_params.discCenterY,
            m_params.discCenterZ + m_params.discOuterRadius * std::sin(angle)
        });
    }

    // 内圈密封(如果是环形盘)
    if (m_params.discInnerRadius > 1.0) {
        for (int i = 0; i < points; ++i) {
            double angle = 2.0 * HvacConst::HVAC_PI * i / points;
            contour.push_back({
                m_params.discCenterX + m_params.discInnerRadius * std::cos(angle),
                m_params.discCenterY,
                m_params.discCenterZ + m_params.discInnerRadius * std::sin(angle)
            });
        }
    }

    return contour;
}

/* ============================================================
 * 力矩计算
 * ============================================================ */

double ModeDiscStrategy::calcTorque(AirMode fromMode, AirMode toMode) const
{
    /**
     * 模式盘操作力矩:
     * M = μ × F_seal × R_mean + M_aero
     *
     * 面密封摩擦:
     *   μ = 0.4 (EPDM橡胶/PP)
     *   F_seal = 密封线压力(N/mm) × 接触周长
     *   R_mean = (外径+内径)/2
     *
     * 气动阻力矩(通常较小, 盘面密封后压差小):
     *   M_aero ≈ 0 (面密封状态)
     */

    double mu = 0.4;
    double sealLinePressure = 0.3; // N/mm (面密封比唇形密封低)
    double R_mean = (m_params.discOuterRadius + m_params.discInnerRadius) / 2.0;

    // 密封接触周长 = 外圈 + 内圈 + 窗口边缘
    double outerCirc = 2.0 * HvacConst::HVAC_PI * m_params.discOuterRadius;
    double innerCirc = 2.0 * HvacConst::HVAC_PI * m_params.discInnerRadius;
    double totalSealLength = outerCirc + innerCirc;

    double M_seal = mu * sealLinePressure * totalSealLength * R_mean * 0.001; // N·mm → N·m转回

    // 转换为N·mm
    double torque_Nmm = mu * sealLinePressure * totalSealLength * (R_mean / 1000.0) * 1000.0;

    // 简化: 返回稳态力矩
    return torque_Nmm;
}

/* ============================================================
 * 模式位置定义
 * ============================================================ */

std::vector<ModePosition> ModeDiscStrategy::getModePositions() const
{
    return {
        {AirMode::FACE,     m_params.angleFace,     "Face ventilation"},
        {AirMode::BI_LEVEL, m_params.angleBiLevel,  "Bi-Level (Face+Foot)"},
        {AirMode::FOOT,     m_params.angleFoot,     "Foot heating"},
        {AirMode::FOOT_DEF, m_params.angleFootDef,  "Foot + Defrost"},
        {AirMode::DEF,      m_params.angleDef,      "Windshield defrost"},
    };
}

/* ============================================================
 * 窗口间距校验
 * ============================================================ */

bool ModeDiscStrategy::validateWindowSpacing() const
{
    // 检查相邻窗口之间是否有足够的密封面(最小5°间隔)
    constexpr double MIN_GAP = 5.0; // deg

    double defEnd = m_params.defWindowStart + m_params.defWindowSpan;
    double faceStart = m_params.faceWindowStart;
    double faceEnd = faceStart + m_params.faceWindowSpan;
    double footStart = m_params.footWindowStart;

    if ((faceStart - defEnd) < MIN_GAP) return false;
    if ((footStart - faceEnd) < MIN_GAP) return false;

    return true;
}

std::vector<ModeDiscStrategy::WindowContour> ModeDiscStrategy::getWindowContours() const
{
    std::vector<WindowContour> contours;

    auto makeContour = [&](const std::string& name, double start, double span,
                           double innerR, double outerR) {
        WindowContour wc;
        wc.name = name;
        int pts = 20;
        // 外弧
        for (int i = 0; i <= pts; ++i) {
            double angle = start + span * i / pts;
            wc.points.push_back({outerR, angle});
        }
        // 内弧(反方向)
        for (int i = pts; i >= 0; --i) {
            double angle = start + span * i / pts;
            wc.points.push_back({innerR, angle});
        }
        contours.push_back(wc);
    };

    makeContour("DEF", m_params.defWindowStart, m_params.defWindowSpan,
                m_params.defWindowInnerR, m_params.defWindowOuterR);
    makeContour("FACE", m_params.faceWindowStart, m_params.faceWindowSpan,
                m_params.faceWindowInnerR, m_params.faceWindowOuterR);
    makeContour("FOOT", m_params.footWindowStart, m_params.footWindowSpan,
                m_params.footWindowInnerR, m_params.footWindowOuterR);

    return contours;
}
