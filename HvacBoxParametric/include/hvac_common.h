/**
 * @file hvac_common.h
 * @brief HVAC箱体参数化系统 - 全局公共定义
 * @details 包含所有模块共用的类型定义、枚举、常量、前向声明。
 *          本文件被所有模块头文件引用，修改时需谨慎评估影响范围。
 *
 *          【重要】NX宏污染修复说明：
 *          NX的uf_defs.h定义了 PI/TWOPI/RADEG/DEGRA 等数学宏，
 *          这些宏会与C++标准数学代码产生冲突。
 *          本文件策略：
 *            1) 先include所有C++标准库头文件（避免被NX宏污染）
 *            2) 再include所有NX头文件（集中管理，其他模块不再直接include）
 *            3) include完毕后 #undef 数学宏 (PI/TWOPI/RADEG/DEGRA/HALFPI)
 *            4) 不动 TRUE/FALSE —— Windows SDK 和 NX 内部均依赖它们
 *               我们的代码中统一使用 true/false (C++ bool) 或
 *               HvacConst::UF_TRUE/UF_FALSE (传给UF API的int参数)
 *
 * @version 1.2
 * @date 2026-05-11
 */

#ifndef HVAC_COMMON_H
#define HVAC_COMMON_H

#include "hvac_nx_version.h"

/* ============================================================
 * C++ 标准库 (必须在NX头文件之前include，避免NX宏污染std)
 * ============================================================ */
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <array>
#include <type_traits>

/* ============================================================
 * NX Open C++ 头文件引用
 * ============================================================ */
#if HVAC_USE_NXOPEN_CPP
    #include <NXOpen/Session.hxx>
    #include <NXOpen/Part.hxx>
    #include <NXOpen/PartCollection.hxx>
    #include <NXOpen/Body.hxx>
    #include <NXOpen/Face.hxx>
    #include <NXOpen/Edge.hxx>
    #include <NXOpen/Point.hxx>
    #include <NXOpen/Expression.hxx>
    #include <NXOpen/ExpressionCollection.hxx>
    #include <NXOpen/Features_Feature.hxx>
    #include <NXOpen/Features_FeatureCollection.hxx>
    #include <NXOpen/Features_ExtrudeBuilder.hxx>
    #include <NXOpen/Features_RevolveBuilder.hxx>
    #include <NXOpen/Features_BooleanBuilder.hxx>
    #include <NXOpen/Features_OffsetSurfaceBuilder.hxx>
    #include <NXOpen/NXException.hxx>
    #include <NXOpen/NXObject.hxx>
    #include <NXOpen/ListingWindow.hxx>
#endif

/* ============================================================
 * UF API 头文件 (全版本兼容)
 * 所有NX/UF头文件必须集中在此处include，项目其他文件
 * 仅通过 #include "hvac_common.h" 间接获得NX API访问。
 * ============================================================ */
#include <uf.h>
#include <uf_defs.h>
#include <uf_modl.h>
#include <uf_modl_primitives.h>
#include <uf_modl_utilities.h>
#include <uf_part.h>
#include <uf_obj.h>
#include <uf_assem.h>
#include <uf_attr.h>
#include <uf_ui.h>
#include <uf_csys.h>
#include <uf_vec.h>
#include <uf_mtx.h>
#include <uf_curve.h>
#include <uf_eval.h>
#include <uf_so.h>
#include <uf_layer.h>
#include <uf_disp.h>

/* ============================================================
 * 清除NX数学宏污染
 *
 * 注意：只清除数学常量宏，不清除 TRUE/FALSE！
 * TRUE/FALSE 是 Windows SDK (<windef.h>) 的标准宏，
 * NX头文件和MSVC STL内部都依赖它们。如果 #undef TRUE/FALSE，
 * 会导致后续 STL 头文件（xlocmon, xloctime, iomanip等）编译失败。
 *
 * 我们的代码中：
 *   - 布尔判断用 C++ 原生 true/false
 *   - 传给 UF API 的 int 参数用 HvacConst::UF_TRUE / UF_FALSE
 * ============================================================ */
#ifdef PI
#undef PI
#endif

#ifdef TWOPI
#undef TWOPI
#endif

#ifdef HALFPI
#undef HALFPI
#endif

#ifdef RADEG
#undef RADEG
#endif

#ifdef DEGRA
#undef DEGRA
#endif

#ifdef R_FACTOR
#undef R_FACTOR
#endif

/* ============================================================
 * 全局常量 (替代被清除的NX宏)
 * ============================================================ */
namespace HvacConst {

    /** 数学常量 - 替代 NX 的 PI/TWOPI/RADEG/DEGRA */
    constexpr double HVAC_PI      = 3.14159265358979323846;
    constexpr double HVAC_TWOPI   = 6.28318530717958647692;
    constexpr double HVAC_HALFPI  = 1.57079632679489661923;
    constexpr double DEG_TO_RAD   = HVAC_PI / 180.0;
    constexpr double RAD_TO_DEG   = 180.0 / HVAC_PI;

    /** 布尔值替代 - 用于UF API调用(UF API参数为int型) */
    constexpr int UF_TRUE  = 1;
    constexpr int UF_FALSE = 0;

    /** 几何公差 */
    constexpr double TOLERANCE_LINEAR  = 0.001;     // 线性公差 0.001mm
    constexpr double TOLERANCE_ANGULAR = 0.01;      // 角度公差 0.01deg
    constexpr double TOLERANCE_DISTANCE = 0.01;     // 距离判断公差 0.01mm

    /** 壳体设计约束 */
    constexpr double MIN_WALL_THICKNESS = 2.0;      // 最小壁厚 mm
    constexpr double MAX_WALL_THICKNESS = 3.5;      // 最大壁厚 mm
    constexpr double MIN_DRAFT_ANGLE = 1.5;         // 最小拔模角 deg
    constexpr double MIN_APPEARANCE_DRAFT = 3.0;    // 外观面最小拔模角 deg
    constexpr double MIN_DOOR_CLEARANCE = 0.5;      // 风门最小间隙 mm
    constexpr double SEAL_GROOVE_MIN_WIDTH = 3.0;   // 密封槽最小宽度 mm
    constexpr double SEAL_GROOVE_MAX_WIDTH = 5.0;   // 密封槽最大宽度 mm
    constexpr double SNAP_FIT_SPACING_MIN = 60.0;   // 卡扣最小间距 mm
    constexpr double SNAP_FIT_SPACING_MAX = 100.0;  // 卡扣最大间距 mm

    /** 运动机构约束 */
    constexpr double TEMP_DOOR_SWEEP_MIN = 45.0;    // 温度风门最小摆角 deg
    constexpr double TEMP_DOOR_SWEEP_MAX = 90.0;    // 温度风门最大摆角 deg
    constexpr double LINKAGE_TRANSMISSION_ANGLE_MIN = 40.0; // 连杆最小传动角 deg
    constexpr double ACTUATOR_SAFETY_FACTOR = 0.7;  // 执行器力矩安全系数

    /** 材料属性 (PP+TD20) */
    constexpr double MATERIAL_DENSITY = 1.04;       // 密度 g/cm3
    constexpr double MATERIAL_SHRINKAGE = 0.005;    // 收缩率 0.5%
    constexpr double MATERIAL_FLEXURAL_MODULUS = 2800.0; // 弯曲模量 MPa

    /** 系统限制 */
    constexpr int MAX_EXPRESSION_NAME_LEN = 64;     // Expression名称最大长度
    constexpr int MAX_UNDO_MARKS = 10;              // 最大Undo标记数

} // namespace HvacConst

/* ============================================================
 * 全局枚举定义
 * ============================================================ */

/** 风门类型 */
enum class DoorType {
    TEMP_DOOR,          // 温度风门
    MODE_DOOR_DEF,      // 模式风门-除霜
    MODE_DOOR_FACE,     // 模式风门-吹面
    MODE_DOOR_FOOT,     // 模式风门-暖足
    INTAKE_DOOR         // 内外循环风门
};

/** 出风口类型 */
enum class OutletType {
    DEF,    // 除霜 Defrost
    FACE,   // 吹面 Face
    FOOT,   // 暖足 Foot
    REAR    // 后排 Rear (可选)
};

/** 壳体分型位置 */
enum class ShellHalf {
    UPPER,  // 上壳体
    LOWER   // 下壳体
};

/** 左右驾定义 */
enum class DriveSide {
    LHD,    // 左舵 (Left Hand Drive)
    RHD     // 右舵 (Right Hand Drive)
};

/** 参数状态 */
enum class ParamStatus {
    VALID,              // 参数在合理范围内
    WARN_NEAR_LIMIT,    // 接近边界(90%~100%范围), 黄色警告
    FAIL_OUT_RANGE,     // 超出硬限制, 拒绝赋值并回退
    FAIL_CONFLICT       // 与其他参数冲突(几何不可实现)
};

/** 校验结果 */
enum class ValidationResult {
    PASS,   // 通过
    WARN,   // 警告(可接受但需注意)
    FAIL    // 失败(必须修正)
};

/** 模块构建状态 */
enum class BuildStatus {
    NOT_STARTED,    // 未开始
    IN_PROGRESS,    // 构建中
    SUCCESS,        // 成功完成
    FAILED,         // 失败
    ROLLED_BACK     // 已回滚
};

/** 参数层级 */
enum class ParamLevel {
    LEVEL_0,    // 整车输入参数 (外部约束, 不可修改)
    LEVEL_1,    // 驱动参数 (设计师主动设定)
    LEVEL_2,    // 关联参数 (自动计算)
    LEVEL_3     // 校验参数 (验证用)
};

/* ============================================================
 * 基础数据结构
 * ============================================================ */

/** 三维点/向量 */
struct Vec3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3d() = default;
    Vec3d(double _x, double _y, double _z) : x(_x), y(_y), z(_z) {}

    Vec3d operator+(const Vec3d& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    Vec3d operator-(const Vec3d& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    Vec3d operator*(double s) const { return {x * s, y * s, z * s}; }
    double length() const { return std::sqrt(x * x + y * y + z * z); }
    double dot(const Vec3d& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z; }
    Vec3d cross(const Vec3d& rhs) const {
        return {y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x};
    }
    Vec3d normalized() const {
        double len = length();
        if (len < HvacConst::TOLERANCE_LINEAR) return {0, 0, 0};
        return {x / len, y / len, z / len};
    }
};

/** 参数范围定义 */
struct ParamRange {
    double minVal = 0.0;
    double maxVal = 0.0;
    double defaultVal = 0.0;
    std::string unit;       // "mm", "deg", "kW" etc.

    bool isInRange(double val) const { return val >= minVal && val <= maxVal; }
    bool isNearLimit(double val, double threshold = 0.9) const {
        double range = maxVal - minVal;
        double distToMin = val - minVal;
        double distToMax = maxVal - val;
        return (distToMin < range * (1.0 - threshold)) || (distToMax < range * (1.0 - threshold));
    }
};

/** 校验条目 */
struct ValidationItem {
    std::string id;             // e.g. "L3-001"
    std::string description;    // description of check
    ValidationResult result = ValidationResult::PASS;
    std::string message;        // result details
    double actualValue = 0.0;   // measured value
    double limitValue = 0.0;    // limit value
};

/** 构建上下文 - 在模块间传递 */
struct BuildContext {
#if HVAC_USE_NXOPEN_CPP
    NXOpen::Session* pSession = nullptr;
    NXOpen::Part* pWorkPart = nullptr;
#endif
    tag_t workPartTag = NULL_TAG;
    tag_t undoMarkId = NULL_TAG;
    BuildStatus status = BuildStatus::NOT_STARTED;
    std::vector<std::string> logMessages;
};

/* ============================================================
 * 实用宏定义
 * ============================================================ */

/** 安全释放UF数组 */
#define HVAC_UF_FREE(ptr) do { if ((ptr) != nullptr) { UF_free(ptr); (ptr) = nullptr; } } while(0)

/** 检查UF返回码 */
#define HVAC_CHECK_UF(rc, msg) \
    do { \
        int _rc = (rc); \
        if (_rc != 0) { \
            char _err_msg[256]; \
            UF_get_fail_message(_rc, _err_msg); \
            throw std::runtime_error(std::string(msg) + " UF Error: " + _err_msg); \
        } \
    } while(0)

/** Expression名称合法性检查(仅允许 [a-zA-Z0-9_]) */
inline bool isValidExpressionName(const std::string& name) {
    if (name.empty() || name.length() > static_cast<size_t>(HvacConst::MAX_EXPRESSION_NAME_LEN)) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

/** 角度转弧度 */
inline double degToRad(double deg) { return deg * HvacConst::DEG_TO_RAD; }

/** 弧度转角度 */
inline double radToDeg(double rad) { return rad * HvacConst::RAD_TO_DEG; }

#endif /* HVAC_COMMON_H */
