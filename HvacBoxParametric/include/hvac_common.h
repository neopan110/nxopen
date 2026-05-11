/**
 * @file hvac_common.h
 * @brief HVAC箱体参数化系统 - 全局公共定义
 * @details 包含所有模块共用的类型定义、枚举、常量、前向声明。
 *          本文件被所有模块头文件引用，修改时需谨慎评估影响范围。
 *
 *          【NX宏污染修复】
 *          策略：
 *            1) C++标准库在NX头文件之前include（避免被NX宏污染std）
 *            2) 所有NX/UF头文件集中在此处include（其他文件不再直接include）
 *            3) include完毕后 #undef 数学宏 (PI/TWOPI/RADEG/DEGRA/HALFPI)
 *            4) 不动 TRUE/FALSE —— Windows SDK和NX内部均依赖它们
 *
 * @version 2.0
 * @date 2026-05-11
 */

#ifndef HVAC_COMMON_H
#define HVAC_COMMON_H

#include "hvac_nx_version.h"

/* ============================================================
 * C++ 标准库 (必须在NX头文件之前)
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
 * NX Open C++ 头文件
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
 * UF API 头文件 (集中管理，其他文件不再直接include)
 * ============================================================ */
#include <uf.h>
#include <uf_defs.h>
#include <uf_modl.h>
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
#include <uf_exit.h>

/* ============================================================
 * 清除NX数学宏（只清数学宏，不动TRUE/FALSE）
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
 * 全局常量
 * ============================================================ */
namespace HvacConst {

    constexpr double HVAC_PI      = 3.14159265358979323846;
    constexpr double HVAC_TWOPI   = 6.28318530717958647692;
    constexpr double HVAC_HALFPI  = 1.57079632679489661923;
    constexpr double DEG_TO_RAD   = HVAC_PI / 180.0;
    constexpr double RAD_TO_DEG   = 180.0 / HVAC_PI;

    constexpr int UF_TRUE  = 1;
    constexpr int UF_FALSE = 0;

    constexpr double TOLERANCE_LINEAR  = 0.001;
    constexpr double TOLERANCE_ANGULAR = 0.01;
    constexpr double TOLERANCE_DISTANCE = 0.01;

    constexpr double MIN_WALL_THICKNESS = 2.0;
    constexpr double MAX_WALL_THICKNESS = 3.5;
    constexpr double MIN_DRAFT_ANGLE = 1.5;
    constexpr double MIN_APPEARANCE_DRAFT = 3.0;
    constexpr double MIN_DOOR_CLEARANCE = 0.5;
    constexpr double SEAL_GROOVE_MIN_WIDTH = 3.0;
    constexpr double SEAL_GROOVE_MAX_WIDTH = 5.0;
    constexpr double SNAP_FIT_SPACING_MIN = 60.0;
    constexpr double SNAP_FIT_SPACING_MAX = 100.0;

    constexpr double TEMP_DOOR_SWEEP_MIN = 45.0;
    constexpr double TEMP_DOOR_SWEEP_MAX = 90.0;
    constexpr double LINKAGE_TRANSMISSION_ANGLE_MIN = 40.0;
    constexpr double ACTUATOR_SAFETY_FACTOR = 0.7;

    constexpr double MATERIAL_DENSITY = 1.04;
    constexpr double MATERIAL_SHRINKAGE = 0.005;
    constexpr double MATERIAL_FLEXURAL_MODULUS = 2800.0;

    constexpr int MAX_EXPRESSION_NAME_LEN = 64;
    constexpr int MAX_UNDO_MARKS = 10;

} // namespace HvacConst

/* ============================================================
 * 枚举定义
 * ============================================================ */

enum class DoorType {
    TEMP_DOOR,
    MODE_DOOR_DEF,
    MODE_DOOR_FACE,
    MODE_DOOR_FOOT,
    INTAKE_DOOR
};

enum class OutletType {
    DEF,
    FACE,
    FOOT,
    REAR
};

enum class ShellHalf {
    UPPER,
    LOWER
};

enum class DriveSide {
    LHD,
    RHD
};

enum class ParamStatus {
    VALID,
    WARN_NEAR_LIMIT,
    FAIL_OUT_RANGE,
    FAIL_CONFLICT
};

enum class ValidationResult {
    PASS,
    WARN,
    FAIL
};

enum class BuildStatus {
    NOT_STARTED,
    IN_PROGRESS,
    SUCCESS,
    FAILED,
    ROLLED_BACK
};

enum class ParamLevel {
    LEVEL_0,
    LEVEL_1,
    LEVEL_2,
    LEVEL_3
};

/* ============================================================
 * 基础数据结构
 * ============================================================ */

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

struct ParamRange {
    double minVal = 0.0;
    double maxVal = 0.0;
    double defaultVal = 0.0;
    std::string unit;

    bool isInRange(double val) const { return val >= minVal && val <= maxVal; }
    bool isNearLimit(double val, double threshold = 0.9) const {
        double range = maxVal - minVal;
        double distToMin = val - minVal;
        double distToMax = maxVal - val;
        return (distToMin < range * (1.0 - threshold)) || (distToMax < range * (1.0 - threshold));
    }
};

struct ValidationItem {
    std::string id;
    std::string description;
    ValidationResult result = ValidationResult::PASS;
    std::string message;
    double actualValue = 0.0;
    double limitValue = 0.0;
};

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
 * 实用宏
 * ============================================================ */

#define HVAC_UF_FREE(ptr) do { if ((ptr) != nullptr) { UF_free(ptr); (ptr) = nullptr; } } while(0)

#define HVAC_CHECK_UF(rc, msg) \
    do { \
        int _rc = (rc); \
        if (_rc != 0) { \
            char _err_msg[256]; \
            UF_get_fail_message(_rc, _err_msg); \
            throw std::runtime_error(std::string(msg) + " UF Error: " + _err_msg); \
        } \
    } while(0)

#define HVAC_CHECK_NULL(ptr, msg) \
    do { \
        if ((ptr) == nullptr) { \
            throw std::runtime_error(std::string(msg)); \
        } \
    } while(0)

/* ============================================================
 * 实用内联函数
 * ============================================================ */

inline bool isValidExpressionName(const std::string& name) {
    if (name.empty() || name.length() > static_cast<size_t>(HvacConst::MAX_EXPRESSION_NAME_LEN)) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

inline double degToRad(double deg) { return deg * HvacConst::DEG_TO_RAD; }
inline double radToDeg(double rad) { return rad * HvacConst::RAD_TO_DEG; }

#endif /* HVAC_COMMON_H */
