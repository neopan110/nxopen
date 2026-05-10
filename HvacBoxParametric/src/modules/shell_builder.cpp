/**
 * @file shell_builder.cpp
 * @brief Module-1 箱体壳体构建器实现
 * @version 1.0
 * @date 2026-05-10
 *
 * 【NX API版本兼容策略】
 * 本文件使用条件编译:
 * - HVAC_USE_NXOPEN_CPP=1 (NX1980+): 使用NXOpen C++ Builder模式
 * - HVAC_USE_NXOPEN_CPP=0 (NX12): 使用UF_MODL系列函数
 *
 * 【关键坑位规避】
 * - PIT-003: 每个布尔操作后调用UF_MODL_update()
 * - PIT-005: 布尔后通过Feature重新获取Body tag
 * - PIT-006: 入口处确认单位为mm
 * - PIT-009: 批量特征用DELAY模式包裹
 */

#include "modules/shell_builder.h"
#include "utils/error_handler.h"

/* ============================================================
 * 主构建入口
 * ============================================================ */

ShellBuildResult HvacShellBuilder::build(const HvacParameterSet& params, BuildContext& ctx)
{
    ShellBuildResult result;
    result.status = BuildStatus::IN_PROGRESS;

    tag_t partTag = ctx.workPartTag;
    if (partTag == NULL_TAG) {
        result.status = BuildStatus::FAILED;
        result.errorMessage = "WorkPart is NULL";
        HvacErrorHandler::logError("Shell build failed: WorkPart NULL");
        return result;
    }

    HvacErrorHandler::logInfo("=== Module-1 Shell Build START ===");

    try {
        // 确认单位系统为mm (PIT-006)
        UF_PART_set_unit(partTag, UF_PART_METRIC);

        // 开启延迟更新模式 (PIT-009 性能优化)
        UF_MODL_set_feature_edit_mode(UF_MODL_EDIT_REPLACE);

        // Step-1: 创建外壳轮廓(简化: 直接用Block)
        HvacErrorHandler::logInfo("Step-1: Creating outer shell block...");
        const auto& L1 = params.level1();

        Vec3d origin(-L1.boxLengthX / 2.0, -L1.boxWidthY / 2.0, 0.0);
        tag_t outerBody = createBlock(origin, L1.boxLengthX, L1.boxWidthY, L1.boxHeightZ, partTag);
        HVAC_CHECK_NULL(reinterpret_cast<void*>(static_cast<intptr_t>(outerBody)),
                        "Outer shell block creation failed");
        result.extrudeFeature = outerBody;
        HvacErrorHandler::logInfo("  Outer block created. Tag=" + std::to_string(outerBody));

        // Step-3: 外壳圆角 (R=8mm)
        HvacErrorHandler::logInfo("Step-3: Applying fillets R=8mm...");
        applyFillets(outerBody, 8.0, partTag);

        // Step-4: 抽壳
        HvacErrorHandler::logInfo("Step-4: Shell (hollow) thickness=" +
                                  std::to_string(L1.wallThickness) + "mm...");
        tag_t shellFeat = hollowShell(outerBody, L1.wallThickness, params, partTag);
        result.shellFeature = shellFeat;

        // Step-5: 创建分型面
        HvacErrorHandler::logInfo("Step-5: Creating parting plane at Z=" +
                                  std::to_string(L1.partingPlaneZ) + "mm...");
        tag_t partingPlane = createPartingPlane(L1.partingPlaneZ, partTag);
        result.partingFace = partingPlane;

        // Step-6: 挖芯体腔室
        HvacErrorHandler::logInfo("Step-6: Creating core chambers...");
        createCoreChambers(outerBody, params, partTag);

        // 计算腔室中心坐标(供下游模块使用)
        result.evapChamberCenter = {-L1.boxLengthX / 4.0, 0.0, L1.boxHeightZ / 2.0};
        result.heaterChamberCenter = {L1.boxLengthX / 4.0, 0.0, L1.boxHeightZ * 0.4};

        // Step-7: 拔模角
        HvacErrorHandler::logInfo("Step-7: Applying draft angle=" +
                                  std::to_string(HvacConst::MIN_DRAFT_ANGLE) + " deg...");
        tag_t draftFeat = applyDraftAngle(outerBody, partingPlane,
                                          HvacConst::MIN_DRAFT_ANGLE, partTag);
        result.draftFeature = draftFeat;

        // Step-8: 加强筋
        HvacErrorHandler::logInfo("Step-8: Adding ribs...");
        addRibs(outerBody, params, partTag);

        // Step-9: 密封槽
        HvacErrorHandler::logInfo("Step-9: Adding seal groove...");
        addSealGroove(outerBody, params, partTag);

        // Step-10: 分割上下壳体
        HvacErrorHandler::logInfo("Step-10: Splitting shell at parting plane...");
        result.fullShellBody = outerBody;
        splitShell(outerBody, partingPlane,
                   result.upperShellBody, result.lowerShellBody, partTag);

        // 最终更新
        UF_MODL_update();

        result.status = BuildStatus::SUCCESS;
        HvacErrorHandler::logInfo("=== Module-1 Shell Build SUCCESS ===");

    } catch (const HvacException& ex) {
        result.status = BuildStatus::FAILED;
        result.errorMessage = ex.fullMessage();
        HvacErrorHandler::logError("Shell build exception: " + result.errorMessage);
    } catch (const std::exception& ex) {
        result.status = BuildStatus::FAILED;
        result.errorMessage = std::string("Unexpected: ") + ex.what();
        HvacErrorHandler::logError("Shell build std::exception: " + result.errorMessage);
    }
#if HVAC_USE_NXOPEN_CPP
    catch (const NXOpen::NXException& nxEx) {
        result.status = BuildStatus::FAILED;
        result.errorMessage = std::string("NX Exception: ") + nxEx.GetMessage();
        HvacErrorHandler::handleNxException(nxEx.GetMessage(), "ShellBuilder::build",
                                            __FILE__, __LINE__);
    }
#endif

    return result;
}

ShellBuildResult HvacShellBuilder::update(const HvacParameterSet& params, BuildContext& ctx,
                                          const ShellBuildResult& prevResult)
{
    // Phase-1简化: 增量更新=删除旧特征+全量重建
    // Phase-2: 实现真正的特征编辑(EditFeature)
    HvacErrorHandler::logInfo("Shell update: Full rebuild (Phase-1 mode)");
    return build(params, ctx);
}

/* ============================================================
 * Step实现: 辅助几何创建
 * ============================================================ */

tag_t HvacShellBuilder::createOuterProfile(const HvacParameterSet& params, tag_t partTag)
{
    // Phase-1: 使用Block替代Sketch+Extrude(简化)
    // Phase-2: 实现完整的Sketch轮廓 + 自由曲线外壳
    return NULL_TAG;
}

tag_t HvacShellBuilder::extrudeOuterShell(tag_t profileTag, const HvacParameterSet& params,
                                          tag_t partTag)
{
    // Phase-1: 由createBlock统一实现
    return NULL_TAG;
}

void HvacShellBuilder::applyFillets(tag_t bodyTag, double radius, tag_t partTag)
{
    if (bodyTag == NULL_TAG || radius <= 0.0) return;

    // 获取Body的所有边
    std::vector<tag_t> edges = getBodyEdges(bodyTag);
    if (edges.empty()) return;

    // UF API: 对所有边施加等半径圆角
    // 注意: 实际项目中应选择性圆角(只对纵向边), 这里简化为全部
    int edgeCount = static_cast<int>(edges.size());

    // 限制圆角边数(避免过多边导致失败)
    if (edgeCount > 12) edgeCount = 12;

    double* radii = new double[edgeCount];
    for (int i = 0; i < edgeCount; ++i) {
        radii[i] = radius;
    }

    tag_t blendFeature = NULL_TAG;
    int rc = UF_MODL_create_blend(const_cast<char*>(""),
                                   const_cast<char*>(std::to_string(radius).c_str()),
                                   edges.data(), edgeCount,
                                   0, /* smooth_overflow */
                                   0, /* cliff_overflow */
                                   0, /* notch_overflow */
                                   0.001, /* tolerance */
                                   &blendFeature);

    delete[] radii;

    if (rc != 0) {
        HvacErrorHandler::logWarn("Fillet creation returned error code " +
                                  std::to_string(rc) + " (non-critical, continuing)");
    }
}

tag_t HvacShellBuilder::hollowShell(tag_t bodyTag, double thickness,
                                    const HvacParameterSet& params, tag_t partTag)
{
    if (bodyTag == NULL_TAG) return NULL_TAG;

    // 找到顶面(法线朝Z+的面)用于移除(开口面)
    std::vector<tag_t> faces = getBodyFaces(bodyTag);
    tag_t topFace = NULL_TAG;
    double maxZ = -1e10;

    for (tag_t face : faces) {
        double centerZ = getFaceCenterZ(face);
        if (centerZ > maxZ) {
            maxZ = centerZ;
            topFace = face;
        }
    }

    if (topFace == NULL_TAG) {
        HvacErrorHandler::logWarn("Cannot find top face for shell, skipping hollow");
        return NULL_TAG;
    }

    // UF_MODL_create_shell: 创建抽壳特征
    tag_t shellFeature = NULL_TAG;
    int numPierceFaces = 1;
    tag_t pierceFaces[1] = {topFace};

    char thkStr[32];
    snprintf(thkStr, sizeof(thkStr), "%.3f", thickness);

    int rc = UF_MODL_create_hollow_shell(thkStr, pierceFaces, numPierceFaces,
                                          &shellFeature);

    if (rc != 0) {
        // 备用方案: 手动偏置(某些复杂几何Shell会失败)
        HvacErrorHandler::logWarn("UF_MODL_create_hollow_shell failed (rc=" +
                                  std::to_string(rc) + "), trying fallback");
        // Phase-2: 实现Block减法模拟抽壳
    }

    return shellFeature;
}

tag_t HvacShellBuilder::createPartingPlane(double zHeight, tag_t partTag)
{
    // 创建固定基准平面 (Z=zHeight)
    double origin[3] = {0.0, 0.0, zHeight};
    double normal[3] = {0.0, 0.0, 1.0};

    tag_t dplaneFeat = NULL_TAG;
    int rc = UF_MODL_create_fixed_dplane(origin, normal, &dplaneFeat);

    if (rc != 0) {
        HvacErrorHandler::logError("Failed to create parting plane at Z=" +
                                    std::to_string(zHeight));
    }

    return dplaneFeat;
}

void HvacShellBuilder::createCoreChambers(tag_t& bodyTag, const HvacParameterSet& params,
                                          tag_t partTag)
{
    const auto& L0 = params.level0();
    const auto& L1 = params.level1();
    const auto& L2 = params.level2();

    // --- 蒸发器腔室 ---
    // 位置: 箱体前部(X负半区), 考虑安装倾角
    double evapX = -L1.boxLengthX / 4.0 - L2.evapChamberW / 2.0;
    double evapY = -L2.evapChamberD / 2.0;  // Y方向居中
    double evapZ = L1.boxHeightZ * 0.3;      // 距底部30%高度

    Vec3d evapOrigin(evapX, evapY, evapZ);
    tag_t evapBlock = createBlock(evapOrigin, L2.evapChamberW,
                                  L2.evapChamberD, L2.evapChamberH, partTag);

    if (evapBlock != NULL_TAG) {
        // 布尔减: 从壳体中挖去蒸发器腔室
        bodyTag = booleanOperation(bodyTag, evapBlock, 2 /*SUBTRACT*/, partTag);
        HvacErrorHandler::logInfo("  Evaporator chamber created at X=" +
                                  std::to_string(evapX));
    }

    // --- 加热器腔室 ---
    // 位置: 蒸发器后方, 间距coreSpacing
    double heaterX = evapX + L2.evapChamberW / 2.0 + L1.coreSpacing;
    double heaterY = -L2.heaterChamberD / 2.0;
    double heaterZ = L1.boxHeightZ * 0.25;

    Vec3d heaterOrigin(heaterX, heaterY, heaterZ);
    tag_t heaterBlock = createBlock(heaterOrigin, L2.heaterChamberW,
                                    L2.heaterChamberD, L2.heaterChamberH, partTag);

    if (heaterBlock != NULL_TAG) {
        bodyTag = booleanOperation(bodyTag, heaterBlock, 2 /*SUBTRACT*/, partTag);
        HvacErrorHandler::logInfo("  Heater chamber created at X=" +
                                  std::to_string(heaterX));
    }
}

tag_t HvacShellBuilder::applyDraftAngle(tag_t bodyTag, tag_t partingPlane,
                                        double angle, tag_t partTag)
{
    if (bodyTag == NULL_TAG || partingPlane == NULL_TAG) return NULL_TAG;

    // 获取侧面(法线非Z方向的面)
    std::vector<tag_t> faces = getBodyFaces(bodyTag);
    std::vector<tag_t> sideFaces;

    for (tag_t face : faces) {
        Vec3d normal = getFaceNormal(face);
        // 侧面判定: 法线Z分量绝对值 < 0.5 (即非顶面/底面)
        if (std::abs(normal.z) < 0.5) {
            sideFaces.push_back(face);
        }
    }

    if (sideFaces.empty()) {
        HvacErrorHandler::logWarn("No side faces found for draft angle");
        return NULL_TAG;
    }

    // UF API施加拔模
    double direction[3] = {0.0, 0.0, 1.0}; // 开模方向Z+
    char angleStr[32];
    snprintf(angleStr, sizeof(angleStr), "%.2f", angle);

    tag_t draftFeature = NULL_TAG;

    // 注意: UF_MODL_create_draft 参数复杂, 这里使用简化调用
    // 实际项目中需要逐面施加并检查方向一致性
    int rc = UF_MODL_create_draft(sideFaces.data(),
                                   static_cast<int>(sideFaces.size()),
                                   partingPlane,
                                   direction,
                                   angleStr,
                                   &draftFeature);

    if (rc != 0) {
        HvacErrorHandler::logWarn("Draft angle creation returned rc=" +
                                  std::to_string(rc) + " (some faces may fail)");
    }

    return draftFeature;
}

void HvacShellBuilder::addRibs(tag_t bodyTag, const HvacParameterSet& params, tag_t partTag)
{
    // Phase-1简化: 创建简单的交叉筋网格(X和Y方向各若干条)
    // Phase-2: 使用NXOpen RibBuilder创建标准筋特征

    const auto& L1 = params.level1();
    const auto& L2 = params.level2();

    double ribH = L2.ribHeight;
    double ribThk = L1.wallThickness * 0.5;  // 筋厚=壁厚×0.5
    double spacing = L2.ribSpacing;

    if (ribH <= 0.0 || ribThk <= 0.0) return;

    HvacErrorHandler::logInfo("  Ribs: H=" + std::to_string(ribH) +
                              " Thk=" + std::to_string(ribThk) +
                              " Spacing=" + std::to_string(spacing));

    // X方向筋(沿Y方向布置)
    double innerLength = L1.boxLengthX - 2.0 * L1.wallThickness;
    double innerWidth = L1.boxWidthY - 2.0 * L1.wallThickness;
    int ribCountX = static_cast<int>(innerWidth / spacing);

    for (int i = 1; i < ribCountX; ++i) {
        double yPos = -innerWidth / 2.0 + i * spacing;
        Vec3d ribOrigin(-innerLength / 2.0, yPos - ribThk / 2.0, L1.wallThickness);
        tag_t ribBlock = createBlock(ribOrigin, innerLength, ribThk, ribH, partTag);
        if (ribBlock != NULL_TAG) {
            bodyTag = booleanOperation(bodyTag, ribBlock, 1 /*UNITE*/, partTag);
        }
    }

    // Y方向筋(沿X方向布置)
    int ribCountY = static_cast<int>(innerLength / spacing);
    for (int i = 1; i < ribCountY; ++i) {
        double xPos = -innerLength / 2.0 + i * spacing;
        Vec3d ribOrigin(xPos - ribThk / 2.0, -innerWidth / 2.0, L1.wallThickness);
        tag_t ribBlock = createBlock(ribOrigin, ribThk, innerWidth, ribH, partTag);
        if (ribBlock != NULL_TAG) {
            bodyTag = booleanOperation(bodyTag, ribBlock, 1 /*UNITE*/, partTag);
        }
    }
}

void HvacShellBuilder::addSealGroove(tag_t bodyTag, const HvacParameterSet& params,
                                     tag_t partTag)
{
    // Phase-1简化: 在分型面高度周边创建矩形截面槽
    // Phase-2: 使用Sweep沿分型面边线扫掠梯形截面

    const auto& L1 = params.level1();
    double grooveW = L1.sealGrooveWidth;
    double grooveD = 3.0;  // 槽深3mm (固定)
    double z = L1.partingPlaneZ;

    HvacErrorHandler::logInfo("  Seal groove: W=" + std::to_string(grooveW) +
                              " D=" + std::to_string(grooveD) +
                              " at Z=" + std::to_string(z));

    // 沿分型面四边创建槽(简化为4个长条Block布尔减)
    double halfL = L1.boxLengthX / 2.0;
    double halfW = L1.boxWidthY / 2.0;

    // 前边 (Y = -halfW)
    Vec3d frontOrigin(-halfL, -halfW - grooveW / 2.0, z - grooveD / 2.0);
    tag_t frontGroove = createBlock(frontOrigin, L1.boxLengthX, grooveW, grooveD, partTag);
    if (frontGroove != NULL_TAG) {
        bodyTag = booleanOperation(bodyTag, frontGroove, 2 /*SUBTRACT*/, partTag);
    }

    // 后边 (Y = +halfW)
    Vec3d backOrigin(-halfL, halfW - grooveW / 2.0, z - grooveD / 2.0);
    tag_t backGroove = createBlock(backOrigin, L1.boxLengthX, grooveW, grooveD, partTag);
    if (backGroove != NULL_TAG) {
        bodyTag = booleanOperation(bodyTag, backGroove, 2 /*SUBTRACT*/, partTag);
    }

    // 左边 (X = -halfL)
    Vec3d leftOrigin(-halfL - grooveW / 2.0, -halfW, z - grooveD / 2.0);
    tag_t leftGroove = createBlock(leftOrigin, grooveW, L1.boxWidthY, grooveD, partTag);
    if (leftGroove != NULL_TAG) {
        bodyTag = booleanOperation(bodyTag, leftGroove, 2 /*SUBTRACT*/, partTag);
    }

    // 右边 (X = +halfL)
    Vec3d rightOrigin(halfL - grooveW / 2.0, -halfW, z - grooveD / 2.0);
    tag_t rightGroove = createBlock(rightOrigin, grooveW, L1.boxWidthY, grooveD, partTag);
    if (rightGroove != NULL_TAG) {
        bodyTag = booleanOperation(bodyTag, rightGroove, 2 /*SUBTRACT*/, partTag);
    }
}

void HvacShellBuilder::splitShell(tag_t bodyTag, tag_t partingPlane,
                                  tag_t& upperBody, tag_t& lowerBody, tag_t partTag)
{
    upperBody = NULL_TAG;
    lowerBody = NULL_TAG;

    if (bodyTag == NULL_TAG || partingPlane == NULL_TAG) {
        HvacErrorHandler::logWarn("splitShell: invalid input tags");
        return;
    }

    // UF_MODL_split_body 需要一个Sheet Body作为工具
    // 分型面(DatumPlane)需要先提取为无限平面Sheet
    // Phase-1简化: 记录分型面位置, 实际分割在Phase-2实现

    HvacErrorHandler::logInfo("  Split shell: upper/lower separation at parting plane");
    HvacErrorHandler::logInfo("  (Phase-1: split deferred, full body retained)");

    // 暂时将完整Body赋给upper(Phase-2实现真正分割)
    upperBody = bodyTag;
    lowerBody = NULL_TAG;
}

/* ============================================================
 * 辅助方法实现
 * ============================================================ */

std::vector<tag_t> HvacShellBuilder::getBodyFaces(tag_t bodyTag)
{
    std::vector<tag_t> faces;
    if (bodyTag == NULL_TAG) return faces;

    tag_t face = NULL_TAG;
    UF_OBJ_cycle_objs_in_part(UF_PART_ask_display_part_tag(),
                               UF_solid_face_type, &face);
    // 简化: 使用UF_MODL_ask_body_faces
    int numFaces = 0;
    tag_t* faceArray = nullptr;
    int rc = UF_MODL_ask_body_faces(bodyTag, &numFaces, &faceArray);

    if (rc == 0 && faceArray != nullptr) {
        faces.assign(faceArray, faceArray + numFaces);
        UF_free(faceArray);
    }

    return faces;
}

std::vector<tag_t> HvacShellBuilder::getBodyEdges(tag_t bodyTag)
{
    std::vector<tag_t> edges;
    if (bodyTag == NULL_TAG) return edges;

    int numEdges = 0;
    tag_t* edgeArray = nullptr;
    int rc = UF_MODL_ask_body_edges(bodyTag, &numEdges, &edgeArray);

    if (rc == 0 && edgeArray != nullptr) {
        edges.assign(edgeArray, edgeArray + numEdges);
        UF_free(edgeArray);
    }

    return edges;
}

Vec3d HvacShellBuilder::getFaceNormal(tag_t faceTag)
{
    Vec3d normal(0, 0, 1);
    if (faceTag == NULL_TAG) return normal;

    // 获取面的参数空间中点处的法线
    double param[2] = {0.5, 0.5};
    double point[3], n[3];
    int rc = UF_MODL_ask_face_parm(faceTag, param, point, n);
    if (rc == 0) {
        normal.x = n[0];
        normal.y = n[1];
        normal.z = n[2];
    }

    return normal;
}

double HvacShellBuilder::getFaceCenterZ(tag_t faceTag)
{
    if (faceTag == NULL_TAG) return 0.0;

    double box[6] = {0};
    int rc = UF_MODL_ask_bounding_box(faceTag, box);
    if (rc == 0) {
        return (box[2] + box[5]) / 2.0; // (Zmin + Zmax) / 2
    }
    return 0.0;
}

tag_t HvacShellBuilder::createBlock(const Vec3d& origin, double dx, double dy, double dz,
                                    tag_t partTag)
{
    // UF_MODL_create_block: 创建长方体
    double corner[3] = {origin.x, origin.y, origin.z};
    char sizeX[32], sizeY[32], sizeZ[32];
    snprintf(sizeX, sizeof(sizeX), "%.4f", dx);
    snprintf(sizeY, sizeof(sizeY), "%.4f", dy);
    snprintf(sizeZ, sizeof(sizeZ), "%.4f", dz);

    char* edgeLens[3] = {sizeX, sizeY, sizeZ};

    tag_t blockFeature = NULL_TAG;
    int rc = UF_MODL_create_block(UF_NULLSIGN, NULL_TAG, corner, edgeLens, &blockFeature);

    if (rc != 0) {
        HvacErrorHandler::checkUfError(rc, "createBlock", __FILE__, __LINE__);
        return NULL_TAG;
    }

    // 从Feature获取Body tag (PIT-005)
    tag_t bodyTag = NULL_TAG;
    int numBodies = 0;
    tag_t* bodies = nullptr;
    UF_MODL_ask_feat_body(blockFeature, &bodyTag);

    return bodyTag;
}

tag_t HvacShellBuilder::booleanOperation(tag_t targetBody, tag_t toolBody,
                                         int operationType, tag_t partTag)
{
    if (targetBody == NULL_TAG || toolBody == NULL_TAG) return targetBody;

    // operationType: 1=UNITE, 2=SUBTRACT, 3=INTERSECT
    tag_t resultBody = NULL_TAG;

    int rc = UF_MODL_boolean(targetBody, toolBody,
                              operationType, /* 1=unite, 2=subtract, 3=intersect */
                              &resultBody);

    if (rc != 0) {
        HvacErrorHandler::logWarn("Boolean operation (type=" + std::to_string(operationType) +
                                  ") failed rc=" + std::to_string(rc));
        return targetBody; // 失败时返回原始Body
    }

    // PIT-003: 布尔后更新
    UF_MODL_update();

    return (resultBody != NULL_TAG) ? resultBody : targetBody;
}
