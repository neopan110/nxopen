/**
 * @file duct_builder.cpp
 * @brief Module-2 风道系统构建器实现
 * @details 构建蒸发器腔→混合腔→各出风口(DEF/FACE/FOOT)的内部风道。
 *
 * 【风道设计准则】(SAE J639 + CFD简化)
 * 1. 截面渐变: 蒸发器后截面积逐渐缩小到出风口(加速气流)
 * 2. 转弯半径: R/D ≥ 1.5 (减少压损)
 * 3. 面积比: 出风口总面积 / 蒸发器迎风面积 = 0.6~0.85
 * 4. 导流板: 转弯处布置导流片减少涡流
 *
 * 【构建流程】
 * 1. 定义风道中心线(Spine Curve): 从蒸发器后→混合腔→各出风口
 * 2. 定义各截面(Guide Sections): 矩形→圆角矩形→圆形过渡
 * 3. 扫掠/放样(Sweep/Through Curves): 沿中心线生成风道壁
 * 4. 布尔减: 从壳体中挖去风道空间
 * 5. 导流板: 在转弯处创建薄板特征
 *
 * @version 2.0
 * @date 2026-05-10
 */

#include "hvac_common.h"
#include "hvac_parameters.h"
#include "utils/error_handler.h"

/* ============================================================
 * 风道截面定义
 * ============================================================ */

struct DuctSection {
    Vec3d center;           ///< 截面中心点
    double width;           ///< 截面宽度 (mm)
    double height;          ///< 截面高度 (mm)
    double cornerRadius;    ///< 截面圆角半径 (mm)
    Vec3d normal;           ///< 截面法线方向
};

struct DuctPath {
    std::string name;               ///< 风道名称 (如 "DEF_duct")
    std::vector<DuctSection> sections; ///< 沿路径的截面序列
    tag_t bodyTag = NULL_TAG;       ///< 生成的风道Body
};

/* ============================================================
 * 风道构建结果
 * ============================================================ */

struct DuctBuildResult {
    BuildStatus status = BuildStatus::NOT_STARTED;
    std::vector<DuctPath> ducts;        ///< 所有风道
    tag_t mixingChamberBody = NULL_TAG; ///< 混合腔Body
    std::vector<Vec3d> ductCenterlines; ///< 风道中心线(供Module-5)
    std::string errorMessage;
};

/* ============================================================
 * 风道构建器
 * ============================================================ */

class HvacDuctBuilder {
public:
    /**
     * @brief 构建所有风道
     * @param params 参数集
     * @param shellBody 壳体Body(用于布尔减)
     * @param ctx 构建上下文
     */
    DuctBuildResult build(const HvacParameterSet& params,
                          tag_t shellBody, BuildContext& ctx)
    {
        DuctBuildResult result;
        result.status = BuildStatus::IN_PROGRESS;
        tag_t partTag = ctx.workPartTag;

        if (partTag == NULL_TAG || shellBody == NULL_TAG) {
            result.status = BuildStatus::FAILED;
            result.errorMessage = "Invalid part or shell body";
            return result;
        }

        HvacErrorHandler::logInfo("=== Module-2 Duct Build START ===");

        const auto& L0 = params.level0();
        const auto& L1 = params.level1();
        const auto& L2 = params.level2();

        try {
            // --- 计算风道关键尺寸 ---
            // 蒸发器迎风面积(基准)
            double evapFaceArea = L0.evapWidth * L0.evapHeight; // mm²

            // 各出风口目标面积(面积比准则)
            double defArea = evapFaceArea * 0.25;   // DEF占25%
            double faceArea = evapFaceArea * 0.35;  // FACE占35%
            double footArea = evapFaceArea * 0.25;  // FOOT占25%
            // 总计85% (15%为壳壁阻挡+密封损失)

            // --- 创建混合腔 ---
            HvacErrorHandler::logInfo("  Creating mixing chamber...");
            double mixX = L1.tempDoorCenterX; // 混合腔位于温度风门后方
            double mixW = L0.evapWidth * 0.9;
            double mixH = L0.evapHeight * 0.8;
            double mixD = L1.coreSpacing * 0.4;

            Vec3d mixOrigin(mixX, -mixW / 2.0, L1.boxHeightZ * 0.2);
            tag_t mixBlock = createDuctBlock(mixOrigin, mixD, mixW, mixH, partTag);

            if (mixBlock != NULL_TAG) {
                // 布尔减: 从壳体挖去混合腔
                tag_t resultBody = NULL_TAG;
                UF_MODL_boolean(shellBody, mixBlock, 2, &resultBody);
                if (resultBody != NULL_TAG) shellBody = resultBody;
                result.mixingChamberBody = mixBlock;
            }

            // --- DEF风道 ---
            HvacErrorHandler::logInfo("  Creating DEF duct...");
            DuctPath defDuct;
            defDuct.name = "DEF_duct";
            double defW = std::sqrt(defArea / 1.5); // 宽高比1.5:1
            double defH = defW * 1.5;

            // DEF风道: 从混合腔顶部向上弯曲到DEF出风口
            Vec3d defStart(mixX + mixD / 2.0, 0, L1.boxHeightZ * 0.6);
            Vec3d defEnd(L1.boxLengthX * 0.4, 0, L1.defOutletZ);

            defDuct.sections.push_back({defStart, mixW * 0.4, mixH * 0.3, 5.0, {1, 0, 0}});
            defDuct.sections.push_back({defEnd, defW, defH, 8.0, {0, 0, 1}});

            tag_t defBody = createDuctFromSections(defDuct, partTag);
            if (defBody != NULL_TAG) {
                tag_t resultBody = NULL_TAG;
                UF_MODL_boolean(shellBody, defBody, 2, &resultBody);
                if (resultBody != NULL_TAG) shellBody = resultBody;
                defDuct.bodyTag = defBody;
            }
            result.ducts.push_back(defDuct);

            // --- FACE风道 ---
            HvacErrorHandler::logInfo("  Creating FACE duct...");
            DuctPath faceDuct;
            faceDuct.name = "FACE_duct";
            double faceW = std::sqrt(faceArea / 1.2);
            double faceH = faceW * 1.2;

            Vec3d faceStart(mixX + mixD / 2.0, 0, L1.boxHeightZ * 0.5);
            Vec3d faceEnd(L1.boxLengthX * 0.45, 0, L1.faceOutletZ);

            faceDuct.sections.push_back({faceStart, mixW * 0.5, mixH * 0.4, 5.0, {1, 0, 0}});
            faceDuct.sections.push_back({faceEnd, faceW, faceH, 10.0, {1, 0, 0}});

            tag_t faceBody = createDuctFromSections(faceDuct, partTag);
            if (faceBody != NULL_TAG) {
                tag_t resultBody = NULL_TAG;
                UF_MODL_boolean(shellBody, faceBody, 2, &resultBody);
                if (resultBody != NULL_TAG) shellBody = resultBody;
                faceDuct.bodyTag = faceBody;
            }
            result.ducts.push_back(faceDuct);

            // --- FOOT风道 ---
            HvacErrorHandler::logInfo("  Creating FOOT duct...");
            DuctPath footDuct;
            footDuct.name = "FOOT_duct";
            double footW = std::sqrt(footArea / 1.0);
            double footH = footW;

            Vec3d footStart(mixX + mixD / 2.0, 0, L1.boxHeightZ * 0.25);
            Vec3d footEnd(L1.boxLengthX * 0.3, 0, L1.footOutletZ);

            footDuct.sections.push_back({footStart, mixW * 0.4, mixH * 0.3, 4.0, {1, 0, 0}});
            footDuct.sections.push_back({footEnd, footW, footH, 8.0, {0, 0, -1}});

            tag_t footBody = createDuctFromSections(footDuct, partTag);
            if (footBody != NULL_TAG) {
                tag_t resultBody = NULL_TAG;
                UF_MODL_boolean(shellBody, footBody, 2, &resultBody);
                if (resultBody != NULL_TAG) shellBody = resultBody;
                footDuct.bodyTag = footBody;
            }
            result.ducts.push_back(footDuct);

            // --- 导流板 ---
            HvacErrorHandler::logInfo("  Adding guide vanes...");
            addGuideVanes(shellBody, params, partTag);

            // --- 中心线记录(供Module-5风门定位) ---
            result.ductCenterlines.push_back(defStart);
            result.ductCenterlines.push_back(faceStart);
            result.ductCenterlines.push_back(footStart);

            UF_MODL_update();
            result.status = BuildStatus::SUCCESS;
            HvacErrorHandler::logInfo("=== Module-2 Duct Build SUCCESS ===");

        } catch (const std::exception& ex) {
            result.status = BuildStatus::FAILED;
            result.errorMessage = ex.what();
            HvacErrorHandler::logError("Duct build failed: " + result.errorMessage);
        }

        return result;
    }

private:
    /**
     * @brief 从截面序列创建风道体(简化: 用Block连接)
     * @details Phase-3简化: 每段用Block表示, Phase-5替换为真正的Sweep/Loft
     */
    tag_t createDuctFromSections(const DuctPath& duct, tag_t partTag)
    {
        if (duct.sections.size() < 2) return NULL_TAG;

        // 简化: 使用起始截面参数创建单个Block表示整段风道
        const auto& startSec = duct.sections.front();
        const auto& endSec = duct.sections.back();

        // 风道长度 = 起点到终点的距离
        double dx = endSec.center.x - startSec.center.x;
        double dz = endSec.center.z - startSec.center.z;
        double length = std::sqrt(dx * dx + dz * dz);

        // 取平均截面尺寸
        double avgW = (startSec.width + endSec.width) / 2.0;
        double avgH = (startSec.height + endSec.height) / 2.0;

        // Block起点
        Vec3d origin(
            std::min(startSec.center.x, endSec.center.x),
            -avgW / 2.0,
            std::min(startSec.center.z, endSec.center.z)
        );

        return createDuctBlock(origin, std::abs(dx) + 10.0, avgW, avgH, partTag);
    }

    /** 创建风道用Block(带圆角) */
    tag_t createDuctBlock(const Vec3d& origin, double dx, double dy, double dz, tag_t partTag)
    {
        double corner[3] = {origin.x, origin.y, origin.z};
        char sx[32], sy[32], sz[32];
        snprintf(sx, sizeof(sx), "%.4f", std::max(dx, 5.0));
        snprintf(sy, sizeof(sy), "%.4f", std::max(dy, 5.0));
        snprintf(sz, sizeof(sz), "%.4f", std::max(dz, 5.0));
        char* edgeLens[3] = {sx, sy, sz};

        tag_t feat = NULL_TAG;
        int rc = UF_MODL_create_block(UF_NULLSIGN, NULL_TAG, corner, edgeLens, &feat);
        if (rc != 0) return NULL_TAG;

        tag_t body = NULL_TAG;
        UF_MODL_ask_feat_body(feat, &body);
        return body;
    }

    /** 添加导流板(转弯处薄板) */
    void addGuideVanes(tag_t shellBody, const HvacParameterSet& params, tag_t partTag)
    {
        // 在混合腔到DEF转弯处添加1~2片导流板
        const auto& L1 = params.level1();

        double vaneThk = 1.5; // 导流板厚度
        double vaneH = 30.0;  // 导流板高度
        double vaneW = L1.boxWidthY * 0.6; // 导流板宽度

        // 导流板位置: 混合腔上部转弯区
        Vec3d vanePos(L1.tempDoorCenterX + 20.0, -vaneW / 2.0, L1.boxHeightZ * 0.55);
        tag_t vaneBody = createDuctBlock(vanePos, vaneThk, vaneW, vaneH, partTag);

        if (vaneBody != NULL_TAG) {
            tag_t resultBody = NULL_TAG;
            UF_MODL_boolean(shellBody, vaneBody, 1 /*UNITE*/, &resultBody);
            // 导流板加入壳体(布尔加)
        }

        HvacErrorHandler::logInfo("    Guide vane added at transition zone");
    }
};
