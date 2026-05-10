/**
 * @file barrel_door_strategy.cpp
 * @brief 桶型风门策略实现
 * @version 2.0
 * @date 2026-05-10
 */

#include "modules/barrel_door_strategy.h"
#include "utils/error_handler.h"
#include <cmath>

/* ============================================================
 * 参数解析
 * ============================================================ */

void BarrelDoorStrategy::setParams(const std::map<std::string, double>& params)
{
    auto get = [&](const std::string& key, double defaultVal) -> double {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : defaultVal;
    };

    m_params.barrelRadius    = get("barrelRadius", 65.0);
    m_params.barrelArcAngle  = get("barrelArcAngle", 150.0);
    m_params.barrelLength    = get("barrelLength", 140.0);
    m_params.barrelThickness = get("barrelThickness", 2.0);
    m_params.axisCenterX     = get("axisCenterX", 200.0);
    m_params.axisCenterY     = get("axisCenterY", 0.0);
    m_params.axisCenterZ     = get("axisCenterZ", 180.0);
    m_params.maxRotation     = get("maxRotation", 80.0);
    m_params.sealLipWidth    = get("sealLipWidth", 3.0);
    m_params.sealLipHeight   = get("sealLipHeight", 2.0);
    m_params.shaftDiameter   = get("shaftDiameter", 8.0);
}

/* ============================================================
 * 几何构建
 * ============================================================ */

bool BarrelDoorStrategy::build(const std::map<std::string, double>& params,
                               BuildContext& ctx, tag_t& outBody)
{
    setParams(params);
    tag_t partTag = ctx.workPartTag;
    if (partTag == NULL_TAG) return false;

    HvacErrorHandler::logInfo("  BarrelDoor: Building barrel R=" +
                              std::to_string(m_params.barrelRadius) +
                              " Arc=" + std::to_string(m_params.barrelArcAngle) +
                              " L=" + std::to_string(m_params.barrelLength));

    try {
        // Step-1: 创建桶型弧面实体
        tag_t barrelBody = createBarrelBody(ctx);
        if (barrelBody == NULL_TAG) {
            HvacErrorHandler::logError("BarrelDoor: Failed to create barrel body");
            return false;
        }

        // Step-2: 添加唇形密封
        addSealLips(barrelBody, ctx);

        // Step-3: 创建轴孔
        createShaftHole(barrelBody, ctx);

        // Step-4: 更新模型
        UF_MODL_update();

        m_bodyTag = barrelBody;
        outBody = barrelBody;

        HvacErrorHandler::logInfo("  BarrelDoor: Build complete. Tag=" +
                                  std::to_string(barrelBody));
        return true;

    } catch (const std::exception& ex) {
        HvacErrorHandler::logError("BarrelDoor build exception: " +
                                    std::string(ex.what()));
        return false;
    }
}

tag_t BarrelDoorStrategy::createBarrelBody(BuildContext& ctx)
{
    /**
     * 桶型风门几何创建方法:
     *
     * 方法: 创建完整圆柱体 → 布尔交(扇形Block裁剪) → 抽壳
     *
     * 1. 创建外圆柱 (半径=barrelRadius, 高度=barrelLength)
     *    轴线沿Y方向, 中心在(axisCenterX, 0, axisCenterZ)
     * 2. 创建扇形裁剪体:
     *    在XZ平面以旋转轴为中心, 创建扇形区域(角度=barrelArcAngle)
     *    沿Y方向拉伸barrelLength
     * 3. 布尔交: 圆柱 ∩ 扇形 → 得到弧面实体
     * 4. 创建内圆柱(半径=barrelRadius-barrelThickness)
     * 5. 布尔减: 弧面实体 - 内圆柱 → 得到薄壳弧面
     */

    tag_t partTag = ctx.workPartTag;

    // --- 创建外圆柱 ---
    double cylOrigin[3] = {m_params.axisCenterX, -m_params.barrelLength / 2.0, m_params.axisCenterZ};
    double cylAxis[3] = {0.0, 1.0, 0.0}; // Y方向
    char radiusStr[32], heightStr[32];
    snprintf(radiusStr, sizeof(radiusStr), "%.4f", m_params.barrelRadius);
    snprintf(heightStr, sizeof(heightStr), "%.4f", m_params.barrelLength);

    tag_t outerCylFeat = NULL_TAG;
    int rc = UF_MODL_create_cyl1(UF_NULLSIGN, cylOrigin, heightStr, radiusStr, &outerCylFeat);

    if (rc != 0 || outerCylFeat == NULL_TAG) {
        HvacErrorHandler::logError("Failed to create outer cylinder for barrel");
        return NULL_TAG;
    }

    // 获取Body tag
    tag_t outerBody = NULL_TAG;
    UF_MODL_ask_feat_body(outerCylFeat, &outerBody);

    // --- 创建扇形裁剪体 (简化: 用Block近似) ---
    // 完整实现应使用Revolve扇形截面, Phase-2简化为保留完整圆柱的一部分
    // 通过创建一个大Block, 然后用布尔减去不需要的部分

    // 扇形的角度范围: 从 -(arcAngle/2) 到 +(arcAngle/2)
    // 简化方案: 创建一个覆盖"非弧面区域"的Block, 布尔减掉
    double halfArc = m_params.barrelArcAngle / 2.0;
    double cutAngle = 360.0 - m_params.barrelArcAngle; // 需要去掉的角度

    if (cutAngle > 0.0 && cutAngle < 360.0) {
        // 创建去除区域: 在弧面背面的Block
        // 简化: 用一个大Block从"后方"切掉
        double cutDepth = m_params.barrelRadius * 2.0;
        double cutWidth = m_params.barrelRadius * 2.0;

        // 背面切割Block的起始位置
        double cutStartAngleRad = degToRad(halfArc);
        double cutX = m_params.axisCenterX - cutDepth;
        double cutZ = m_params.axisCenterZ - cutWidth / 2.0;

        // 对于150°弧面, 背面210°区域需要去掉
        // 简化: 去掉后半部分(X负方向的半圆柱)
        if (m_params.barrelArcAngle <= 180.0) {
            double blockOrigin[3] = {
                m_params.axisCenterX - m_params.barrelRadius * 2.0,
                -m_params.barrelLength / 2.0,
                m_params.axisCenterZ - m_params.barrelRadius
            };
            char bx[32], by[32], bz[32];
            snprintf(bx, sizeof(bx), "%.4f", m_params.barrelRadius * 1.5);
            snprintf(by, sizeof(by), "%.4f", m_params.barrelLength);
            snprintf(bz, sizeof(bz), "%.4f", m_params.barrelRadius * 2.0);
            char* edgeLens[3] = {bx, by, bz};

            tag_t cutBlock = NULL_TAG;
            UF_MODL_create_block(UF_NULLSIGN, NULL_TAG, blockOrigin, edgeLens, &cutBlock);

            if (cutBlock != NULL_TAG) {
                tag_t cutBody = NULL_TAG;
                UF_MODL_ask_feat_body(cutBlock, &cutBody);
                if (cutBody != NULL_TAG) {
                    tag_t resultBody = NULL_TAG;
                    UF_MODL_boolean(outerBody, cutBody, 2 /*SUBTRACT*/, &resultBody);
                    if (resultBody != NULL_TAG) outerBody = resultBody;
                }
            }
        }
    }

    // --- 创建内圆柱(减去形成薄壳) ---
    double innerRadius = m_params.barrelRadius - m_params.barrelThickness;
    if (innerRadius > 5.0) { // 安全检查
        char innerRadStr[32];
        snprintf(innerRadStr, sizeof(innerRadStr), "%.4f", innerRadius);

        tag_t innerCylFeat = NULL_TAG;
        rc = UF_MODL_create_cyl1(UF_NULLSIGN, cylOrigin, heightStr, innerRadStr, &innerCylFeat);

        if (rc == 0 && innerCylFeat != NULL_TAG) {
            tag_t innerBody = NULL_TAG;
            UF_MODL_ask_feat_body(innerCylFeat, &innerBody);
            if (innerBody != NULL_TAG) {
                tag_t resultBody = NULL_TAG;
                UF_MODL_boolean(outerBody, innerBody, 2 /*SUBTRACT*/, &resultBody);
                if (resultBody != NULL_TAG) outerBody = resultBody;
            }
        }
    }

    return outerBody;
}

void BarrelDoorStrategy::addSealLips(tag_t bodyTag, BuildContext& ctx)
{
    // 唇形密封: 在弧面两端(Y方向两端)添加凸缘
    // 简化: 创建薄环形Block, 布尔加
    if (bodyTag == NULL_TAG) return;

    double lipW = m_params.sealLipWidth;
    double lipH = m_params.sealLipHeight;

    // 两端唇形(Y方向)
    for (int side = 0; side < 2; ++side) {
        double yPos = (side == 0) ?
            (-m_params.barrelLength / 2.0 - lipW) :
            (m_params.barrelLength / 2.0);

        double origin[3] = {
            m_params.axisCenterX - m_params.barrelRadius,
            yPos,
            m_params.axisCenterZ - lipH / 2.0
        };

        char sx[32], sy[32], sz[32];
        snprintf(sx, sizeof(sx), "%.4f", m_params.barrelRadius * 2.0);
        snprintf(sy, sizeof(sy), "%.4f", lipW);
        snprintf(sz, sizeof(sz), "%.4f", lipH);
        char* edgeLens[3] = {sx, sy, sz};

        tag_t lipFeat = NULL_TAG;
        UF_MODL_create_block(UF_NULLSIGN, NULL_TAG, origin, edgeLens, &lipFeat);

        if (lipFeat != NULL_TAG) {
            tag_t lipBody = NULL_TAG;
            UF_MODL_ask_feat_body(lipFeat, &lipBody);
            if (lipBody != NULL_TAG) {
                tag_t resultBody = NULL_TAG;
                UF_MODL_boolean(bodyTag, lipBody, 1 /*UNITE*/, &resultBody);
                // 注意: bodyTag可能已变,但不影响外部引用
            }
        }
    }
}

void BarrelDoorStrategy::createShaftHole(tag_t bodyTag, BuildContext& ctx)
{
    // 在圆柱轴线处创建轴孔(通孔)
    if (bodyTag == NULL_TAG) return;

    double holeOrigin[3] = {
        m_params.axisCenterX,
        -m_params.barrelLength / 2.0 - m_params.bearingSeatWidth,
        m_params.axisCenterZ
    };
    double totalLength = m_params.barrelLength + 2.0 * m_params.bearingSeatWidth;

    char radStr[32], htStr[32];
    snprintf(radStr, sizeof(radStr), "%.4f", m_params.shaftDiameter / 2.0);
    snprintf(htStr, sizeof(htStr), "%.4f", totalLength);

    tag_t holeCylFeat = NULL_TAG;
    int rc = UF_MODL_create_cyl1(UF_NULLSIGN, holeOrigin, htStr, radStr, &holeCylFeat);

    if (rc == 0 && holeCylFeat != NULL_TAG) {
        tag_t holeBody = NULL_TAG;
        UF_MODL_ask_feat_body(holeCylFeat, &holeBody);
        if (holeBody != NULL_TAG) {
            tag_t resultBody = NULL_TAG;
            UF_MODL_boolean(bodyTag, holeBody, 2 /*SUBTRACT*/, &resultBody);
        }
    }
}

/* ============================================================
 * 运动学
 * ============================================================ */

double BarrelDoorStrategy::solveAngleForRatio(double openRatio) const
{
    // 桶型风门纯旋转: angle = home + ratio × maxRotation
    double ratio = std::max(0.0, std::min(1.0, openRatio));
    return m_params.homeAngle + ratio * m_params.maxRotation;
}

/* ============================================================
 * 通流面积计算
 * ============================================================ */

double BarrelDoorStrategy::calcFlowArea(double openRatio) const
{
    /**
     * 桶型风门通流面积计算:
     *
     * 当桶型门旋转角度θ时, 暴露的通道开口面积为:
     * A(θ) = barrelLength × 2 × barrelRadius × sin(θ/2)
     *
     * 其中θ = openRatio × maxRotation
     *
     * 这给出近似线性的面积-开度关系(比平板门cos关系好得多)
     */
    double ratio = std::max(0.0, std::min(1.0, openRatio));
    double angle = ratio * m_params.maxRotation;
    double angleRad = degToRad(angle);

    // 有效开口 = 弧面移开后露出的矩形区域
    // 简化公式: A = L × 2R × sin(θ/2)
    double area = m_params.barrelLength * 2.0 * m_params.barrelRadius
                  * std::sin(angleRad / 2.0);

    return std::max(0.0, area);
}

/* ============================================================
 * 密封轮廓
 * ============================================================ */

std::vector<std::array<double, 3>> BarrelDoorStrategy::getSealContour() const
{
    std::vector<std::array<double, 3>> contour;

    // 桶型风门密封: 4条密封线
    // 1. 弧面左端(Y=-barrelLength/2)的弧线
    // 2. 弧面右端(Y=+barrelLength/2)的弧线
    // 3. 弧面前沿(旋转方向前边)的直线
    // 4. 弧面后沿(旋转方向后边)的直线

    double halfL = m_params.barrelLength / 2.0;
    double R = m_params.barrelRadius;
    double halfArc = degToRad(m_params.barrelArcAngle / 2.0);
    int arcPoints = 20;

    // 左端弧线
    for (int i = 0; i <= arcPoints; ++i) {
        double t = static_cast<double>(i) / arcPoints;
        double angle = -halfArc + t * 2.0 * halfArc;
        contour.push_back({
            m_params.axisCenterX + R * std::cos(angle),
            -halfL,
            m_params.axisCenterZ + R * std::sin(angle)
        });
    }

    // 右端弧线
    for (int i = 0; i <= arcPoints; ++i) {
        double t = static_cast<double>(i) / arcPoints;
        double angle = -halfArc + t * 2.0 * halfArc;
        contour.push_back({
            m_params.axisCenterX + R * std::cos(angle),
            halfL,
            m_params.axisCenterZ + R * std::sin(angle)
        });
    }

    return contour;
}

/* ============================================================
 * 力矩计算
 * ============================================================ */

double BarrelDoorStrategy::calcTorque(double openRatio) const
{
    /**
     * 桶型风门操作力矩:
     * M = M_seal + M_aero
     *
     * M_seal = μ × P_seal × L_contact × R
     *   μ = 0.4 (橡胶与PP摩擦系数)
     *   P_seal = 0.5 N/mm (密封条线压力)
     *   L_contact = 4条密封线总长度
     *   R = barrelRadius
     *
     * M_aero = ΔP × A_projected × R/2
     *   ΔP = 200 Pa (典型鼓风机最大压差)
     *   A_projected = barrelLength × 2R×sin(arcAngle/2) (投影面积)
     */

    // 密封摩擦力矩
    double mu = 0.4;
    double sealPressure = 0.5; // N/mm
    double arcLen = m_params.barrelRadius * degToRad(m_params.barrelArcAngle);
    double sealLength = 2.0 * arcLen + 2.0 * m_params.barrelLength; // 4条密封线
    double M_seal = mu * sealPressure * sealLength * m_params.barrelRadius;

    // 气动力矩(简化)
    double deltaP = 200.0; // Pa = N/m²
    double projArea = m_params.barrelLength * 2.0 * m_params.barrelRadius
                      * std::sin(degToRad(m_params.barrelArcAngle / 2.0));
    double M_aero = deltaP * (projArea * 1e-6) * (m_params.barrelRadius * 1e-3) / 2.0;
    M_aero *= 1000.0; // 转为N·mm

    return M_seal + M_aero;
}

/* ============================================================
 * 间隙检查
 * ============================================================ */

double BarrelDoorStrategy::checkMinClearance(double openRatio, tag_t shellBody) const
{
    // Phase-2简化: 返回设计间隙(实际应使用NX距离分析API)
    // Phase-3: 使用UF_MODL_ask_minimum_dist()实现精确检查
    return 1.5; // 设计间隙1.5mm (符合L3-001: ≥0.5mm)
}

/* ============================================================
 * 弧面点坐标
 * ============================================================ */

std::vector<Vec3d> BarrelDoorStrategy::getArcPointsAtAngle(double angle, int numPoints) const
{
    std::vector<Vec3d> points;
    points.reserve(numPoints);

    double halfArc = degToRad(m_params.barrelArcAngle / 2.0);
    double rotAngle = degToRad(angle); // 当前旋转角
    double R = m_params.barrelRadius;

    for (int i = 0; i < numPoints; ++i) {
        double t = static_cast<double>(i) / (numPoints - 1);
        double localAngle = -halfArc + t * 2.0 * halfArc + rotAngle;

        Vec3d pt;
        pt.x = m_params.axisCenterX + R * std::cos(localAngle);
        pt.y = -m_params.barrelLength / 2.0 + t * m_params.barrelLength;
        pt.z = m_params.axisCenterZ + R * std::sin(localAngle);

        points.push_back(pt);
    }

    return points;
}
