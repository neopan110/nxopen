/**
 * @file shell_builder.h
 * @brief HVAC箱体参数化系统 - Module-1 箱体壳体构建器
 * @details 基于Level-1驱动参数构建箱体外壳基础几何体，包含：
 *          - 箱体外壳整体拉伸(简化为圆角矩形截面)
 *          - 内腔挖空(壁厚偏置)
 *          - 主分型面生成(上下壳体分割)
 *          - 蒸发器/加热器腔室开口
 *          - 拔模角施加
 *          - 加强筋阵列
 *          - 密封槽特征
 *
 * 【NX API使用策略】
 * - NX2306/NX1980: 优先使用NXOpen::Features::ExtrudeBuilder等
 * - NX12: 使用UF_MODL_create_extrusion()等UF API
 * - 所有版本: 通过HVAC_USE_NXOPEN_CPP宏条件编译
 *
 * 【模具工艺约束】
 * - 所有外壁面拔模角≥1.5°(外观面≥3°)
 * - 壁厚均匀性比值≥0.7
 * - 加强筋高度=壁厚×0.6, 根部厚度=壁厚×0.5
 * - 分型面在Z方向40%~60%高度处
 *
 * @version 1.0
 * @date 2026-05-10
 */

#ifndef HVAC_SHELL_BUILDER_H
#define HVAC_SHELL_BUILDER_H

#include "hvac_common.h"
#include "hvac_parameters.h"
#include "utils/error_handler.h"

/* ============================================================
 * 壳体构建结果
 * ============================================================ */

/**
 * @struct ShellBuildResult
 * @brief Module-1构建输出
 * @details 供下游模块(Module-2风道, Module-3接口, Module-4密封)使用
 */
struct ShellBuildResult {
    BuildStatus status = BuildStatus::NOT_STARTED;

    // 几何输出 (NX tag)
    tag_t upperShellBody = NULL_TAG;    ///< 上壳体实体
    tag_t lowerShellBody = NULL_TAG;    ///< 下壳体实体
    tag_t fullShellBody = NULL_TAG;     ///< 分割前的完整壳体(调试用)
    tag_t partingFace = NULL_TAG;       ///< 主分型面(Sheet Body)
    tag_t innerCavityFace = NULL_TAG;   ///< 内腔参考曲面

    // 特征tag (用于后续编辑/更新)
    tag_t extrudeFeature = NULL_TAG;    ///< 外壳拉伸特征
    tag_t shellFeature = NULL_TAG;      ///< 抽壳特征
    tag_t splitFeature = NULL_TAG;      ///< 分割特征
    tag_t draftFeature = NULL_TAG;      ///< 拔模特征

    // 关键坐标 (供Module-3使用)
    Vec3d evapChamberCenter = {};       ///< 蒸发器腔室中心
    Vec3d heaterChamberCenter = {};     ///< 加热器腔室中心

    std::string errorMessage;
};

/* ============================================================
 * 壳体构建器类
 * ============================================================ */

/**
 * @class HvacShellBuilder
 * @brief Module-1 箱体壳体几何构建器
 *
 * 【构建流程】
 * 1. createOuterProfile()   - 创建外壳截面轮廓(Sketch)
 * 2. extrudeOuterShell()    - 拉伸为实体
 * 3. applyFillets()         - 外壳圆角
 * 4. hollowShell()          - 抽壳(壁厚偏置)
 * 5. createPartingPlane()   - 生成分型面
 * 6. createCoreChambers()   - 挖蒸发器/加热器腔室
 * 7. applyDraftAngle()      - 施加拔模角
 * 8. addRibs()              - 加强筋阵列
 * 9. addSealGroove()        - 密封槽
 * 10. splitShell()          - 分割为上下壳体
 */
class HvacShellBuilder {
public:
    HvacShellBuilder() = default;
    ~HvacShellBuilder() = default;

    /**
     * @brief 执行完整壳体构建
     * @param params 参数集合(Level-0 + Level-1 + Level-2)
     * @param ctx 构建上下文(包含WorkPart等NX环境)
     * @return 构建结果
     */
    ShellBuildResult build(const HvacParameterSet& params, BuildContext& ctx);

    /**
     * @brief 仅更新壳体(参数变更后增量重建)
     * @param params 更新后的参数集
     * @param ctx 构建上下文
     * @param prevResult 上一次构建结果(用于特征编辑)
     * @return 新的构建结果
     */
    ShellBuildResult update(const HvacParameterSet& params, BuildContext& ctx,
                            const ShellBuildResult& prevResult);

private:
    /* ---- 构建子步骤 ---- */

    /**
     * @brief Step-1: 创建外壳截面轮廓
     * @details 在XZ平面创建圆角矩形截面(箱体纵截面)
     *          宽=boxLengthX, 高=boxHeightZ, 圆角R=8mm
     *
     * 【NX API】
     * - NX1980+: NXOpen::SketchInPlaceBuilder + Line/Arc
     * - NX12: UF_MODL_create_line / UF_MODL_create_arc
     */
    tag_t createOuterProfile(const HvacParameterSet& params, tag_t partTag);

    /**
     * @brief Step-2: 拉伸外壳
     * @details 沿Y方向拉伸boxWidthY, 生成实体Box
     *
     * 【NX API - NX2306】
     * NXOpen::Features::ExtrudeBuilder:
     *   builder->SetDistanceOne(距离表达式)
     *   builder->SetDirection(Y轴)
     *   builder->Commit()
     *
     * 【NX API - NX12】
     * UF_MODL_create_extrusion(section, taper, limits, ...)
     * 注意: UF版本不支持Expression直接绑定, 需手动关联
     */
    tag_t extrudeOuterShell(tag_t profileTag, const HvacParameterSet& params, tag_t partTag);

    /**
     * @brief Step-3: 外壳圆角
     * @details 对所有边施加R5圆角(注塑件标准)
     *
     * 【NX API】
     * - NX1980+: NXOpen::Features::EdgeBlendBuilder
     * - NX12: UF_MODL_create_blend()
     * 注意: 选边策略使用UF_MODL_ask_body_edges获取所有边
     */
    void applyFillets(tag_t bodyTag, double radius, tag_t partTag);

    /**
     * @brief Step-4: 抽壳(壁厚偏置)
     * @details 移除顶面, 内偏置wallThickness, 形成空腔
     *
     * 【NX API - NX2306】
     * NXOpen::Features::ShellBuilder:
     *   builder->SetDefaultThickness(wallThickness表达式)
     *   builder->AddPierceface(顶面) // 移除面
     *   builder->Commit()
     *
     * 【NX API - NX12】
     * UF_MODL_create_shell(bodyTag, faces_to_remove, thickness, ...)
     *
     * 【坑位 PIT-009】大面积抽壳可能失败, 需检查返回状态
     */
    tag_t hollowShell(tag_t bodyTag, double thickness,
                      const HvacParameterSet& params, tag_t partTag);

    /**
     * @brief Step-5: 生成主分型面
     * @details 在partingPlaneZ高度创建XY平面(Datum Plane)作为分型面
     *
     * 【NX API】
     * - NX1980+: NXOpen::Features::DatumPlaneBuilder (通过偏置XY平面)
     * - NX12: UF_MODL_create_fixed_dplane()
     */
    tag_t createPartingPlane(double zHeight, tag_t partTag);

    /**
     * @brief Step-6: 挖芯体腔室
     * @details 在箱体内部创建蒸发器和加热器的安装腔室(通过布尔减)
     *          腔室尺寸 = 芯体尺寸 + 间隙(Level-2参数)
     *          考虑芯体安装倾角
     *
     * 【NX API】
     * - 创建腔室Block → 旋转倾角 → 布尔减运算
     * - NX1980+: ExtrudeBuilder + BooleanBuilder(Subtract)
     * - NX12: UF_MODL_create_block + UF_MODL_boolean(SUBTRACT)
     *
     * 【坑位 PIT-005】布尔后原Body tag可能失效, 需重新获取
     */
    void createCoreChambers(tag_t& bodyTag, const HvacParameterSet& params, tag_t partTag);

    /**
     * @brief Step-7: 施加拔模角
     * @details 对外壳所有侧面施加拔模角(最小1.5°)
     *          拔模方向: Z轴正方向(开模方向)
     *          分型面作为拔模中性面
     *
     * 【NX API - NX2306】
     * NXOpen::Features::DraftBuilder:
     *   builder->SetAngle(拔模角度表达式)
     *   builder->SetDraftDirection(Z向量)
     *   builder->SetStationaryPlane(分型面)
     *   选择需要拔模的面
     *   builder->Commit()
     *
     * 【NX API - NX12】
     * UF_MODL_create_draft(faces, direction, angle, ...)
     */
    tag_t applyDraftAngle(tag_t bodyTag, tag_t partingPlane, double angle, tag_t partTag);

    /**
     * @brief Step-8: 加强筋阵列
     * @details 在壳体内壁添加网格状加强筋
     *          筋高=壁厚×0.6, 筋厚=壁厚×0.5, 间距=40mm
     *          筋底部带R0.5圆角(脱模要求)
     *
     * 【NX API】
     * - 创建筋截面(矩形+底部圆角) → 沿路径扫掠
     * - NX1980+: Features::RibBuilder (专用筋特征)
     * - NX12: 扫掠体 UF_MODL_create_swept + 布尔加
     *
     * 【DFM规则】筋高/壁厚 ≤ 0.7, 筋厚/壁厚 ≤ 0.6
     */
    void addRibs(tag_t bodyTag, const HvacParameterSet& params, tag_t partTag);

    /**
     * @brief Step-9: 密封槽
     * @details 在分型面位置沿壳体周边创建密封槽
     *          槽截面: 梯形(便于密封条安装), 宽=sealGrooveWidth, 深=3mm
     *
     * 【NX API】
     * - 创建槽截面 → 沿分型面边线扫掠 → 布尔减
     * - NX1980+: Features::GrooveBuilder 或 Sweep+Boolean
     * - NX12: UF_MODL_create_rectangular_groove 或 Sweep+Boolean
     */
    void addSealGroove(tag_t bodyTag, const HvacParameterSet& params, tag_t partTag);

    /**
     * @brief Step-10: 分割为上下壳体
     * @details 使用分型面将完整壳体分割为上/下两个Body
     *
     * 【NX API - NX2306】
     * NXOpen::Features::SplitBodyBuilder:
     *   builder->SetTarget(完整壳体Body)
     *   builder->SetTool(分型面Sheet)
     *   builder->Commit()
     *   → 获取两个结果Body
     *
     * 【NX API - NX12】
     * UF_MODL_split_body(bodyTag, toolSheetTag, ...)
     *
     * 【坑位】分割后需通过位置判断哪个是上/哪个是下
     */
    void splitShell(tag_t bodyTag, tag_t partingPlane,
                    tag_t& upperBody, tag_t& lowerBody, tag_t partTag);

    /* ---- 辅助方法 ---- */

    /** 获取Body的所有面 */
    std::vector<tag_t> getBodyFaces(tag_t bodyTag);

    /** 获取Body的所有边 */
    std::vector<tag_t> getBodyEdges(tag_t bodyTag);

    /** 判断面的法线方向(用于选择顶面/底面/侧面) */
    Vec3d getFaceNormal(tag_t faceTag);

    /** 获取面的中心点Z坐标(用于上下壳体判断) */
    double getFaceCenterZ(tag_t faceTag);

    /** 创建Block实体(基础几何) */
    tag_t createBlock(const Vec3d& origin, double dx, double dy, double dz, tag_t partTag);

    /** 布尔运算(加/减/交) */
    tag_t booleanOperation(tag_t targetBody, tag_t toolBody,
                           int operationType, tag_t partTag);
};

#endif /* HVAC_SHELL_BUILDER_H */
