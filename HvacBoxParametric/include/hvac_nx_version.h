/**
 * @file hvac_nx_version.h
 * @brief NX版本适配宏定义
 * @details 编译时检测NX版本号，自动启用/禁用版本相关特性。
 *          兼容范围: NX12.0 / NX1980 / NX2306
 * @version 1.0
 * @date 2026-05-10
 *
 * 【版本兼容说明】
 * - NX12:  仅支持UF legacy API，C++11
 * - NX1980: NXOpen C++ 为主 + UF补充，C++14
 * - NX2306: 全NXOpen C++，完整Motion API，C++17
 *
 * 【使用方法】
 * 在NX UGOPEN的编译环境中，NX_VERSION_NUMBER 由头文件自动定义。
 * 若手动编译测试，可通过 -DNX_VERSION_NUMBER=2306000 指定。
 */

#ifndef HVAC_NX_VERSION_H
#define HVAC_NX_VERSION_H

/* ============================================================
 * NX版本号检测 - 若环境未定义则默认NX2306
 * ============================================================ */
#ifndef NX_VERSION_NUMBER
    #define NX_VERSION_NUMBER 2306000
    #pragma message("WARNING: NX_VERSION_NUMBER not defined by environment, defaulting to 2306000")
#endif

/* ============================================================
 * 特性开关宏
 * ============================================================ */

/**
 * HVAC_USE_NXOPEN_CPP: 是否使用NXOpen C++ API (NX1980+)
 * NX12只能用UF API; NX1980及以上优先使用NXOpen C++
 */
#if NX_VERSION_NUMBER >= 1980000
    #define HVAC_USE_NXOPEN_CPP 1
#else
    #define HVAC_USE_NXOPEN_CPP 0
#endif

/**
 * HVAC_USE_JOURNALIDENTIFIER: 是否支持JournalIdentifier (NX1980+)
 * 用于通过名称精确定位NX对象，比tag更稳定
 */
#if NX_VERSION_NUMBER >= 1980000
    #define HVAC_USE_JOURNALIDENTIFIER 1
#else
    #define HVAC_USE_JOURNALIDENTIFIER 0
#endif

/**
 * HVAC_USE_MODERN_EXPRESSION: 是否使用现代Expression API (NX1980+)
 * NXOpen::ExpressionCollection::CreateExpression() 替代 UF_MODL_create_exp()
 */
#if NX_VERSION_NUMBER >= 1980000
    #define HVAC_USE_MODERN_EXPRESSION 1
#else
    #define HVAC_USE_MODERN_EXPRESSION 0
#endif

/**
 * HVAC_USE_NXOPEN_MOTION: 是否使用完整NXOpen Motion API (NX2306+)
 * NX1980的Motion API功能有限，NX2306提供完整运动仿真接口
 */
#if NX_VERSION_NUMBER >= 2306000
    #define HVAC_USE_NXOPEN_MOTION 1
#else
    #define HVAC_USE_NXOPEN_MOTION 0
#endif

/**
 * HVAC_USE_BLOCK_STYLER: 是否使用Block Styler对话框 (NX1980+)
 * NX12需要使用UIStyler或纯UF对话框
 */
#if NX_VERSION_NUMBER >= 1980000
    #define HVAC_USE_BLOCK_STYLER 1
#else
    #define HVAC_USE_BLOCK_STYLER 0
#endif

/**
 * HVAC_USE_ENHANCED_BOOLEAN: 增强布尔运算 (NX2306+)
 * NX2306提供更好的布尔运算错误报告和自动修复能力
 */
#if NX_VERSION_NUMBER >= 2306000
    #define HVAC_USE_ENHANCED_BOOLEAN 1
#else
    #define HVAC_USE_ENHANCED_BOOLEAN 0
#endif

/* ============================================================
 * 编译器特性检测
 * ============================================================ */

/**
 * C++标准版本检测
 * NX2306: C++17, NX1980: C++14, NX12: C++11
 */
#if __cplusplus >= 201703L
    #define HVAC_CPP17_AVAILABLE 1
#else
    #define HVAC_CPP17_AVAILABLE 0
#endif

#if __cplusplus >= 201402L
    #define HVAC_CPP14_AVAILABLE 1
#else
    #define HVAC_CPP14_AVAILABLE 0
#endif

/* ============================================================
 * 平台检测
 * ============================================================ */
#if defined(_WIN32) || defined(_WIN64)
    #define HVAC_PLATFORM_WINDOWS 1
    #define HVAC_PLATFORM_LINUX   0
#elif defined(__linux__)
    #define HVAC_PLATFORM_WINDOWS 0
    #define HVAC_PLATFORM_LINUX   1
#else
    #error "Unsupported platform. Only Windows and Linux are supported."
#endif

/* ============================================================
 * DLL导出宏
 * ============================================================ */
#if HVAC_PLATFORM_WINDOWS
    #ifdef HVAC_EXPORTS
        #define HVAC_API __declspec(dllexport)
    #else
        #define HVAC_API __declspec(dllimport)
    #endif
#else
    #define HVAC_API __attribute__((visibility("default")))
#endif

#endif /* HVAC_NX_VERSION_H */
