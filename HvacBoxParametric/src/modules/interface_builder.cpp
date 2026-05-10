/**
 * @file interface_builder.cpp
 * @brief Module-3 安装接口构建器实现
 * @details 创建所有安装特征:
 *   - 蒸发器/加热器芯体安装导轨与定位特征
 *   - 鼓风机蜗壳安装法兰
 *   - 出风口连接法兰(对接风管)
 *   - 整车安装支架/硬点接口
 *   - 执行器安装座(模式盘+桶型门各一)
 *   - 线束过孔/传感器安装孔
 *
 * 【安装接口设计准则】
 * 1. 芯体导轨: 两侧U型槽, 宽=芯体厚度+0.5mm间隙, 深≥10mm
 * 2. 法兰连接: M5/M6螺钉, 孔距60~80mm, 法兰厚≥5mm
 * 3. 整车硬点: 偏差≤±0.5mm, 安装孔Φ8+衬套
 * 4. 执行器座: 3点定位, 2螺钉固定, 轴孔同轴度≤0.1mm
 *
 * @version 2.0
 * @date 2026-05-10
 */

#include "hvac_common.h"
#include "hvac_parameters.h"
#include "utils/error_handler.h"

/* ============================================================
 * 安装特征结果
 * ============================================================ */

struct InterfaceBuildResult {
    BuildStatus status = BuildStatus::NOT_STARTED;

    // 各安装特征tag
    tag_t evapGuideL = NULL_TAG;        ///< 蒸发器左导轨
    tag_t evapGuideR = NULL_TAG;        ///< 蒸发器右导轨
    tag_t heaterGuideL = NULL_TAG;      ///< 加热器左导轨
    tag_t heaterGuideR = NULL_TAG;      ///< 加热器右导轨
    tag_t blowerFlange = NULL_TAG;      ///< 鼓风机安装法兰
    tag_t defFlange = NULL_TAG;         ///< DEF出风口法兰
    tag_t faceFlange = NULL_TAG;        ///< FACE出风口法兰
    tag_t footFlange = NULL_TAG;        ///< FOOT出风口法兰
    tag_t vehicleMountA = NULL_TAG;     ///< 整车安装点A
    tag_t vehicleMountB = NULL_TAG;     ///< 整车安装点B
    tag_t vehicleMountC = NULL_TAG;     ///< 整车安装点C
    std::vector<tag_t> actuatorSeats;   ///< 执行器安装座

    std::string errorMessage;
};

/* ============================================================
 * 安装接口构建器
 * ============================================================ */

class HvacInterfaceBuilder {
public:
    InterfaceBuildResult build(const HvacParameterSet& params,
                               tag_t shellBody, BuildContext& ctx)
    {
        InterfaceBuildResult result;
        result.status = BuildStatus::IN_PROGRESS;
        tag_t partTag = ctx.workPartTag;

        if (partTag == NULL_TAG || shellBody == NULL_TAG) {
            result.status = BuildStatus::FAILED;
            result.errorMessage = "Invalid part or shell body";
            return result;
        }

        HvacErrorHandler::logInfo("=== Module-3 Interface Build START ===");

        const auto& L0 = params.level0();
        const auto& L1 = params.level1();

        try {
            // --- 1. 蒸发器安装导轨 ---
            HvacErrorHandler::logInfo("  Creating evaporator guide rails...");
            createCoreGuideRails(shellBody, L0.evapDepth, L0.evapHeight,
                                -L1.boxLengthX / 4.0, L1, partTag,
                                result.evapGuideL, result.evapGuideR);

            // --- 2. 加热器安装导轨 ---
            HvacErrorHandler::logInfo("  Creating heater guide rails...");
            double heaterX = -L1.boxLengthX / 4.0 + L1.coreSpacing;
            createCoreGuideRails(shellBody, L0.heaterDepth, L0.heaterHeight,
                                heaterX, L1, partTag,
                                result.heaterGuideL, result.heaterGuideR);

            // --- 3. 鼓风机安装法兰 ---
            HvacErrorHandler::logInfo("  Creating blower flange...");
            result.blowerFlange = createBlowerFlange(
                shellBody, L0.blowerDiameter, L1, partTag);

            // --- 4. 出风口法兰 ---
            HvacErrorHandler::logInfo("  Creating outlet flanges...");
            result.defFlange = createOutletFlange(
                shellBody, "DEF", 50.0, 30.0,
                L1.boxLengthX * 0.4, L1.defOutletZ, L1, partTag);
            result.faceFlange = createOutletFlange(
                shellBody, "FACE", 60.0, 40.0,
                L1.boxLengthX * 0.45, L1.faceOutletZ, L1, partTag);
            result.footFlange = createOutletFlange(
                shellBody, "FOOT", 55.0, 35.0,
                L1.boxLengthX * 0.3, L1.footOutletZ, L1, partTag);

            // --- 5. 整车安装点 ---
            HvacErrorHandler::logInfo("  Creating vehicle mount points...");
            createVehicleMounts(shellBody, L1, partTag,
                               result.vehicleMountA,
                               result.vehicleMountB,
                               result.vehicleMountC);

            // --- 6. 执行器安装座 ---
            HvacErrorHandler::logInfo("  Creating actuator seats...");
            // 温度风门执行器(左侧壁外)
            tag_t tempActuatorSeat = createActuatorSeat(
                shellBody,
                Vec3d(L1.tempDoorCenterX, -L1.boxWidthY / 2.0 - L1.actuatorOffsetY,
                      L1.tempDoorCenterZ),
                L1, partTag);
            if (tempActuatorSeat != NULL_TAG)
                result.actuatorSeats.push_back(tempActuatorSeat);

            // 模式盘执行器(右侧壁外)
            tag_t modeActuatorSeat = createActuatorSeat(
                shellBody,
                Vec3d(L1.modeDoorCenterX, L1.boxWidthY / 2.0 + L1.actuatorOffsetY,
                      L1.modeDoorCenterZ),
                L1, partTag);
            if (modeActuatorSeat != NULL_TAG)
                result.actuatorSeats.push_back(modeActuatorSeat);

            // --- 7. 线束过孔 ---
            HvacErrorHandler::logInfo("  Creating wire harness holes...");
            createWiringHoles(shellBody, L1, partTag);

            UF_MODL_update();
            result.status = BuildStatus::SUCCESS;
            HvacErrorHandler::logInfo("=== Module-3 Interface Build SUCCESS ===");

        } catch (const std::exception& ex) {
            result.status = BuildStatus::FAILED;
            result.errorMessage = ex.what();
            HvacErrorHandler::logError("Interface build failed: " + result.errorMessage);
        }

        return result;
    }

private:
    /**
     * @brief 创建芯体安装导轨(U型槽)
     * @details 在壳体内壁两侧创建竖直U型槽，芯体从顶部插入
     *
     * 【NX API】UF_MODL_create_block → Boolean subtract
     */
    void createCoreGuideRails(tag_t shellBody, double coreDepth, double coreHeight,
                               double centerX, const HvacLevel1Params& L1,
                               tag_t partTag, tag_t& leftGuide, tag_t& rightGuide)
    {
        double guideWidth = coreDepth + 0.5; // 芯体厚+间隙
        double guideDepth = 10.0;            // 导轨深度
        double guideHeight = coreHeight + 20.0; // 导轨高度(比芯体高20mm)

        double innerWidth = L1.boxWidthY - 2.0 * L1.wallThickness;

        // 左导轨
        Vec3d leftPos(centerX - guideWidth / 2.0,
                      -innerWidth / 2.0,
                      L1.wallThickness);
        leftGuide = createBlock(leftPos, guideWidth, guideDepth, guideHeight, partTag);
        if (leftGuide != NULL_TAG) {
            tag_t res = NULL_TAG;
            UF_MODL_boolean(shellBody, leftGuide, 1 /*UNITE*/, &res);
        }

        // 右导轨
        Vec3d rightPos(centerX - guideWidth / 2.0,
                       innerWidth / 2.0 - guideDepth,
                       L1.wallThickness);
        rightGuide = createBlock(rightPos, guideWidth, guideDepth, guideHeight, partTag);
        if (rightGuide != NULL_TAG) {
            tag_t res = NULL_TAG;
            UF_MODL_boolean(shellBody, rightGuide, 1 /*UNITE*/, &res);
        }
    }

    /**
     * @brief 创建鼓风机安装法兰
     * @details 圆形法兰，在箱体侧面或后面开圆孔+凸台
     */
    tag_t createBlowerFlange(tag_t shellBody, double blowerDia,
                             const HvacLevel1Params& L1, tag_t partTag)
    {
        // 鼓风机安装在箱体一端(X负方向端面)
        double flangeThk = 5.0;
        double flangeOD = blowerDia + 20.0; // 法兰外径=蜗壳+20mm

        // 创建法兰凸台(圆柱)
        double origin[3] = {
            -L1.boxLengthX / 2.0 - flangeThk,
            0,
            L1.boxHeightZ * 0.5
        };
        char radStr[32], htStr[32];
        snprintf(radStr, sizeof(radStr), "%.4f", flangeOD / 2.0);
        snprintf(htStr, sizeof(htStr), "%.4f", flangeThk);

        tag_t flangeFeat = NULL_TAG;
        UF_MODL_create_cyl1(UF_NULLSIGN, origin, htStr, radStr, &flangeFeat);

        if (flangeFeat != NULL_TAG) {
            tag_t flangeBody = NULL_TAG;
            UF_MODL_ask_feat_body(flangeFeat, &flangeBody);
            if (flangeBody != NULL_TAG) {
                tag_t res = NULL_TAG;
                UF_MODL_boolean(shellBody, flangeBody, 1 /*UNITE*/, &res);
            }

            // 中心通孔(蜗壳出口)
            char holeRad[32];
            snprintf(holeRad, sizeof(holeRad), "%.4f", blowerDia / 2.0);
            char holeHt[32];
            snprintf(holeHt, sizeof(holeHt), "%.4f", flangeThk + L1.wallThickness + 5.0);

            tag_t holeFeat = NULL_TAG;
            UF_MODL_create_cyl1(UF_NULLSIGN, origin, holeHt, holeRad, &holeFeat);
            if (holeFeat != NULL_TAG) {
                tag_t holeBody = NULL_TAG;
                UF_MODL_ask_feat_body(holeFeat, &holeBody);
                if (holeBody != NULL_TAG) {
                    tag_t res = NULL_TAG;
                    UF_MODL_boolean(shellBody, holeBody, 2 /*SUBTRACT*/, &res);
                }
            }

            return flangeFeat;
        }
        return NULL_TAG;
    }

    /**
     * @brief 创建出风口法兰
     * @details 矩形开口+法兰凸台+螺钉孔
     */
    tag_t createOutletFlange(tag_t shellBody, const std::string& name,
                              double width, double height,
                              double posX, double posZ,
                              const HvacLevel1Params& L1, tag_t partTag)
    {
        double flangeThk = 4.0;
        double flangeMargin = 8.0; // 法兰边宽

        // 法兰凸台(壳体外表面凸出)
        Vec3d flangePos(posX - (width + 2 * flangeMargin) / 2.0,
                        -L1.boxWidthY / 2.0 - flangeThk,
                        posZ - (height + 2 * flangeMargin) / 2.0);
        tag_t flangeBody = createBlock(flangePos,
                                        width + 2 * flangeMargin,
                                        flangeThk,
                                        height + 2 * flangeMargin, partTag);

        if (flangeBody != NULL_TAG) {
            tag_t res = NULL_TAG;
            UF_MODL_boolean(shellBody, flangeBody, 1 /*UNITE*/, &res);
        }

        // 中心通孔
        Vec3d holePos(posX - width / 2.0,
                      -L1.boxWidthY / 2.0 - flangeThk - 1.0,
                      posZ - height / 2.0);
        tag_t holeBody = createBlock(holePos, width, flangeThk + L1.wallThickness + 2.0,
                                      height, partTag);
        if (holeBody != NULL_TAG) {
            tag_t res = NULL_TAG;
            UF_MODL_boolean(shellBody, holeBody, 2 /*SUBTRACT*/, &res);
        }

        HvacErrorHandler::logInfo("    Outlet [" + name + "] flange at X=" +
                                  std::to_string(posX) + " Z=" + std::to_string(posZ));
        return flangeBody;
    }

    /**
     * @brief 创建整车安装点
     * @details 3点定位: 前下方2点 + 后上方1点, 三角形布局
     */
    void createVehicleMounts(tag_t shellBody, const HvacLevel1Params& L1,
                              tag_t partTag,
                              tag_t& mountA, tag_t& mountB, tag_t& mountC)
    {
        // 安装凸台: 圆柱Φ20×10mm + 中心Φ8通孔
        double bossR = 10.0;
        double bossH = 10.0;
        double holeR = 4.0;

        struct MountPoint { double x, y, z; };
        MountPoint mounts[3] = {
            {-L1.boxLengthX * 0.35, -L1.boxWidthY / 2.0 - bossH, L1.wallThickness + 10},
            { L1.boxLengthX * 0.35, -L1.boxWidthY / 2.0 - bossH, L1.wallThickness + 10},
            { 0, -L1.boxWidthY / 2.0 - bossH, L1.boxHeightZ * 0.7}
        };
        tag_t* results[3] = {&mountA, &mountB, &mountC};

        for (int i = 0; i < 3; ++i) {
            double origin[3] = {mounts[i].x, mounts[i].y, mounts[i].z};
            char radStr[32], htStr[32];
            snprintf(radStr, sizeof(radStr), "%.4f", bossR);
            snprintf(htStr, sizeof(htStr), "%.4f", bossH);

            tag_t bossFeat = NULL_TAG;
            UF_MODL_create_cyl1(UF_NULLSIGN, origin, htStr, radStr, &bossFeat);
            if (bossFeat != NULL_TAG) {
                tag_t bossBody = NULL_TAG;
                UF_MODL_ask_feat_body(bossFeat, &bossBody);
                if (bossBody != NULL_TAG) {
                    tag_t res = NULL_TAG;
                    UF_MODL_boolean(shellBody, bossBody, 1, &res);
                }
                *results[i] = bossFeat;
            }

            // 安装通孔
            char holeRadStr[32];
            snprintf(holeRadStr, sizeof(holeRadStr), "%.4f", holeR);
            char holeHtStr[32];
            snprintf(holeHtStr, sizeof(holeHtStr), "%.4f", bossH + L1.wallThickness + 5.0);

            tag_t holeFeat = NULL_TAG;
            UF_MODL_create_cyl1(UF_NULLSIGN, origin, holeHtStr, holeRadStr, &holeFeat);
            if (holeFeat != NULL_TAG) {
                tag_t holeBody = NULL_TAG;
                UF_MODL_ask_feat_body(holeFeat, &holeBody);
                if (holeBody != NULL_TAG) {
                    tag_t res = NULL_TAG;
                    UF_MODL_boolean(shellBody, holeBody, 2, &res);
                }
            }
        }

        HvacErrorHandler::logInfo("    3 vehicle mount points created (triangle layout)");
    }

    /**
     * @brief 创建执行器安装座
     * @details 方形凸台+3点定位孔+轴孔
     */
    tag_t createActuatorSeat(tag_t shellBody, const Vec3d& position,
                              const HvacLevel1Params& L1, tag_t partTag)
    {
        // 执行器安装座: 40×40×8mm方形凸台
        double seatW = 40.0, seatH = 40.0, seatD = 8.0;
        Vec3d seatOrigin(position.x - seatW / 2.0,
                         position.y - seatD / 2.0,
                         position.z - seatH / 2.0);
        tag_t seatBody = createBlock(seatOrigin, seatW, seatD, seatH, partTag);

        if (seatBody != NULL_TAG) {
            tag_t res = NULL_TAG;
            UF_MODL_boolean(shellBody, seatBody, 1 /*UNITE*/, &res);

            // 中心轴孔 Φ6
            double origin[3] = {position.x, position.y - seatD, position.z};
            char radStr[32], htStr[32];
            snprintf(radStr, sizeof(radStr), "3.0");
            snprintf(htStr, sizeof(htStr), "%.4f", seatD + L1.wallThickness + 5.0);

            tag_t holeFeat = NULL_TAG;
            UF_MODL_create_cyl1(UF_NULLSIGN, origin, htStr, radStr, &holeFeat);
            if (holeFeat != NULL_TAG) {
                tag_t holeBody = NULL_TAG;
                UF_MODL_ask_feat_body(holeFeat, &holeBody);
                if (holeBody != NULL_TAG) {
                    UF_MODL_boolean(shellBody, holeBody, 2, &res);
                }
            }
        }

        return seatBody;
    }

    /** 创建线束/传感器过孔 */
    void createWiringHoles(tag_t shellBody, const HvacLevel1Params& L1, tag_t partTag)
    {
        // 2个线束过孔(Φ12): 壳体侧壁, 接近执行器位置
        double holeR = 6.0;
        double positions[2][3] = {
            {L1.tempDoorCenterX + 30.0, -L1.boxWidthY / 2.0 - 5.0, L1.tempDoorCenterZ - 20.0},
            {L1.modeDoorCenterX + 30.0,  L1.boxWidthY / 2.0 - 5.0, L1.modeDoorCenterZ - 20.0}
        };

        for (int i = 0; i < 2; ++i) {
            char radStr[32], htStr[32];
            snprintf(radStr, sizeof(radStr), "%.4f", holeR);
            snprintf(htStr, sizeof(htStr), "%.4f", L1.wallThickness + 10.0);

            tag_t holeFeat = NULL_TAG;
            UF_MODL_create_cyl1(UF_NULLSIGN, positions[i], htStr, radStr, &holeFeat);
            if (holeFeat != NULL_TAG) {
                tag_t holeBody = NULL_TAG;
                UF_MODL_ask_feat_body(holeFeat, &holeBody);
                if (holeBody != NULL_TAG) {
                    tag_t res = NULL_TAG;
                    UF_MODL_boolean(shellBody, holeBody, 2, &res);
                }
            }
        }
    }

    /** 辅助: 创建Block */
    tag_t createBlock(const Vec3d& origin, double dx, double dy, double dz, tag_t partTag)
    {
        double corner[3] = {origin.x, origin.y, origin.z};
        char sx[32], sy[32], sz[32];
        snprintf(sx, sizeof(sx), "%.4f", dx);
        snprintf(sy, sizeof(sy), "%.4f", dy);
        snprintf(sz, sizeof(sz), "%.4f", dz);
        char* edgeLens[3] = {sx, sy, sz};

        tag_t feat = NULL_TAG;
        UF_MODL_create_block(UF_NULLSIGN, NULL_TAG, corner, edgeLens, &feat);
        if (feat == NULL_TAG) return NULL_TAG;

        tag_t body = NULL_TAG;
        UF_MODL_ask_feat_body(feat, &body);
        return body;
    }
};
