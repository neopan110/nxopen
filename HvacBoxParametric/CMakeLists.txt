################################################################################
# CMakeLists.txt - HVAC Box Parametric NX2306 Plugin
#
# 编译步骤:
#   1. 设置NX环境: set UGII_BASE_DIR=C:\Siemens\NX2306
#   2. mkdir build && cd build
#   3. cmake .. -G "Visual Studio 16 2019" -A x64
#   4. cmake --build . --config Release
#   5. 复制 bin/hvac_box_parametric.dll 到 NX application目录
#
# 兼容: NX12 / NX1980 / NX2306
# 编译器要求: VS2019 v16.x (平台工具集v142)
# 注意: 不兼容VS2022! (PIT-011)
################################################################################

cmake_minimum_required(VERSION 3.16)
project(HvacBoxParametric
    VERSION 1.0.0
    DESCRIPTION "HVAC Box Parametric Design Plugin for Siemens NX"
    LANGUAGES CXX
)

# ============================================================
# C++ 标准
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# NX 环境检测
# ============================================================
if(DEFINED ENV{UGII_BASE_DIR})
    set(NX_BASE_DIR $ENV{UGII_BASE_DIR})
elseif(DEFINED UGII_BASE_DIR)
    set(NX_BASE_DIR ${UGII_BASE_DIR})
else()
    message(FATAL_ERROR
        "UGII_BASE_DIR not set!\n"
        "Please set the NX installation path:\n"
        "  Windows: set UGII_BASE_DIR=C:\\Siemens\\NX2306\n"
        "  Linux:   export UGII_BASE_DIR=/opt/Siemens/NX2306\n"
        "Or pass -DUGII_BASE_DIR=... to cmake."
    )
endif()

set(NX_UGOPEN_DIR "${NX_BASE_DIR}/UGOPEN")
message(STATUS "NX Base Dir: ${NX_BASE_DIR}")
message(STATUS "NX UGOPEN Dir: ${NX_UGOPEN_DIR}")

# 检查UGOPEN目录存在
if(NOT EXISTS "${NX_UGOPEN_DIR}")
    message(FATAL_ERROR "UGOPEN directory not found: ${NX_UGOPEN_DIR}")
endif()

# ============================================================
# NX版本检测 (通过头文件)
# ============================================================
# 默认假设NX2306, 用户可覆盖: -DNX_VERSION=2306000
if(NOT DEFINED NX_VERSION)
    set(NX_VERSION 2306000)
endif()
message(STATUS "Target NX Version: ${NX_VERSION}")

# ============================================================
# 头文件路径
# ============================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
    ${NX_UGOPEN_DIR}
    ${NX_UGOPEN_DIR}/NXOpen
    ${NX_UGOPEN_DIR}/NXOpen/Features
)

# ============================================================
# NX 库路径
# ============================================================
if(WIN32)
    link_directories(
        "${NX_BASE_DIR}/NXBIN"
        "${NX_BASE_DIR}/NXBIN/managed"
    )
else()
    link_directories(
        "${NX_BASE_DIR}/ugii"
    )
endif()

# ============================================================
# NX 链接库
# ============================================================
set(NX_LIBS
    libufun          # UF legacy API
    libnxopencpp     # NXOpen C++ API
    libugopenint     # NX Open internal utilities
)

# ============================================================
# 源文件列表
# ============================================================
set(HVAC_HEADERS
    include/hvac_common.h
    include/hvac_nx_version.h
    include/hvac_parameters.h
    include/core/parameter_engine.h
    include/modules/shell_builder.h
    include/utils/error_handler.h
    include/utils/math_solver.h
)

set(HVAC_SOURCES
    src/main/hvac_entry_point.cpp
    src/core/hvac_parameters.cpp
    src/core/parameter_engine.cpp
    src/modules/shell_builder.cpp
    src/utils/error_handler.cpp
    src/utils/math_solver.cpp
)

# ============================================================
# 目标: 动态链接库 (NX插件)
# ============================================================
add_library(HvacBoxParametric SHARED
    ${HVAC_SOURCES}
    ${HVAC_HEADERS}
)

target_link_libraries(HvacBoxParametric PRIVATE ${NX_LIBS})

# ============================================================
# 编译定义
# ============================================================
target_compile_definitions(HvacBoxParametric PRIVATE
    HVAC_EXPORTS                    # DLL导出宏
    NX_VERSION_NUMBER=${NX_VERSION} # NX版本号
)

if(WIN32)
    target_compile_definitions(HvacBoxParametric PRIVATE
        WNT             # NX Windows平台标记
        _UNICODE
        UNICODE
        WIN64
        _WIN64
    )
endif()

# ============================================================
# 编译选项
# ============================================================
if(MSVC)
    target_compile_options(HvacBoxParametric PRIVATE
        /W3             # 警告级别3
        /EHsc           # C++异常处理
        /MP             # 多核编译
        /Zc:__cplusplus # 正确报告__cplusplus宏
        /utf-8          # 源文件和执行字符集均为UTF-8,消除C4819
    )

    # Release优化
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")

    # 禁用特定警告(NX头文件产生的)
    target_compile_options(HvacBoxParametric PRIVATE
        /wd4251   # dll-interface warning (NX类导出)
        /wd4275   # non dll-interface class used as base
        /wd4996   # deprecated functions
    )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(HvacBoxParametric PRIVATE
        -Wall -Wextra -Wpedantic
        -fPIC
        -Wno-unused-parameter
    )
endif()

# ============================================================
# 输出配置
# ============================================================
set_target_properties(HvacBoxParametric PROPERTIES
    OUTPUT_NAME "hvac_box_parametric"
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# 对于Release/Debug多配置生成器
set_target_properties(HvacBoxParametric PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
)

# ============================================================
# 安装规则 (可选)
# ============================================================
install(TARGETS HvacBoxParametric
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(FILES config/param_defaults.json
    DESTINATION bin
)

# ============================================================
# 打印构建信息
# ============================================================
message(STATUS "")
message(STATUS "=== HVAC Box Parametric Build Configuration ===")
message(STATUS "  NX Version:    ${NX_VERSION}")
message(STATUS "  NX Base:       ${NX_BASE_DIR}")
message(STATUS "  C++ Standard:  C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output:        ${CMAKE_BINARY_DIR}/bin/hvac_box_parametric.dll")
message(STATUS "================================================")
message(STATUS "")
