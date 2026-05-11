@echo off
chcp 65001 >nul 2>&1
title HVAC Box Parametric - Build Script
REM ================================================================
REM  HVAC Box Parametric - 一键编译脚本
REM  使用方法: 双击本文件即可编译
REM  前提条件: 已安装VS2019 + NX2306环境变量已设置
REM
REM  Win11注意: 如果双击闪退, 请右键本文件 -> "以管理员身份运行"
REM             或者右键 -> "在终端中打开" 后手动输入: build.bat
REM ================================================================

echo.
echo ==========================================
echo   HVAC Box Parametric Builder v1.0
echo   One-Click Build Script
echo ==========================================
echo.

REM --- 检查NX环境 ---
if not defined UGII_BASE_DIR (
    echo ============================================================
    echo  [ERROR] UGII_BASE_DIR 环境变量未设置!
    echo ============================================================
    echo.
    echo  请先设置NX2306安装路径:
    echo.
    echo  方法1 (永久生效):
    echo    右键"此电脑" - 属性 - 高级系统设置 - 环境变量
    echo    新建系统变量:
    echo      变量名: UGII_BASE_DIR
    echo      变量值: C:\Siemens\NX2306
    echo    (改成你实际的NX安装路径)
    echo    设置后重启电脑或重新打开命令行窗口
    echo.
    echo  方法2 (临时, 仅当前窗口有效):
    echo    在本窗口输入以下命令后重新运行build.bat:
    echo      set UGII_BASE_DIR=C:\Siemens\NX2306
    echo      build.bat
    echo.
    echo ============================================================
    echo.
    echo 按任意键关闭...
    pause >nul
    goto :eof
)

echo [OK] NX Base Dir: %UGII_BASE_DIR%

REM --- 验证NX目录存在 ---
if not exist "%UGII_BASE_DIR%\UGOPEN" (
    echo.
    echo [ERROR] NX UGOPEN directory not found!
    echo   Expected: %UGII_BASE_DIR%\UGOPEN
    echo.
    echo   Please check UGII_BASE_DIR path is correct.
    echo   Your NX2306 installation should have a UGOPEN subfolder.
    echo.
    echo 按任意键关闭...
    pause >nul
    goto :eof
)

echo [OK] NX UGOPEN Dir exists.

REM --- 检查VS2019 ---
set "VS_PATH="
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
)
if not defined VS_PATH (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
        set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    )
)
if not defined VS_PATH (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
        set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    )
)

if not defined VS_PATH (
    echo.
    echo ============================================================
    echo  [ERROR] Visual Studio 2019 未找到!
    echo ============================================================
    echo.
    echo  请安装 VS2019 Community (免费版):
    echo  https://visualstudio.microsoft.com/vs/older-downloads/
    echo.
    echo  安装时勾选: "使用C++的桌面开发"
    echo.
    echo  注意: 不要安装VS2022! NX2306不兼容VS2022.
    echo.
    echo ============================================================
    echo.
    echo 按任意键关闭...
    pause >nul
    goto :eof
)

echo [OK] VS2019 found.

REM --- 初始化VS环境 ---
echo.
echo [INFO] Initializing VS2019 x64 environment...
call "%VS_PATH%" x64 >nul 2>&1

if %errorlevel% neq 0 (
    echo [WARN] VS environment init returned non-zero, continuing anyway...
)

REM --- 检查cmake ---
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo ============================================================
    echo  [ERROR] CMake 未找到!
    echo ============================================================
    echo.
    echo  解决方法 (选一个):
    echo.
    echo  方法1: 重新运行VS2019安装程序, 勾选:
    echo         "用于Windows的C++ CMake工具"
    echo.
    echo  方法2: 单独安装CMake:
    echo         https://cmake.org/download/
    echo         下载 Windows x64 Installer (.msi)
    echo         安装时勾选 "Add CMake to PATH"
    echo.
    echo ============================================================
    echo.
    echo 按任意键关闭...
    pause >nul
    goto :eof
)
echo [OK] CMake found.

REM --- 创建build目录 ---
echo.
echo [INFO] Creating build directory...
if not exist build mkdir build
cd /d build

REM --- CMake配置 ---
echo.
echo [INFO] Running CMake configure...
echo        (This may take 30 seconds on first run)
echo.
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DNX_VERSION=2306000

if %errorlevel% neq 0 (
    echo.
    echo ============================================================
    echo  [ERROR] CMake configure FAILED!
    echo ============================================================
    echo.
    echo  请检查上方的错误信息。常见原因:
    echo  - UGII_BASE_DIR 路径不正确
    echo  - NX2306 UGOPEN 目录中缺少头文件
    echo.
    cd /d ..
    echo 按任意键关闭...
    pause >nul
    goto :eof
)

echo.
echo [OK] CMake configure successful.

REM --- 编译 ---
echo.
echo [INFO] Building (Release mode)...
echo        (First build takes 1-2 minutes)
echo.
cmake --build . --config Release -- /m

if %errorlevel% neq 0 (
    echo.
    echo ============================================================
    echo  [ERROR] Build FAILED!
    echo ============================================================
    echo.
    echo  请检查上方的编译错误信息。
    echo  常见原因:
    echo  - NX头文件版本不匹配
    echo  - 缺少NX库文件(.lib)
    echo.
    cd /d ..
    echo 按任意键关闭...
    pause >nul
    goto :eof
)

echo.
echo ==========================================
echo   BUILD SUCCESSFUL!
echo ==========================================
echo.
echo Output file:
echo   %cd%\bin\Release\hvac_box_parametric.dll
echo.

REM --- 检查输出文件是否存在 ---
set "DLL_PATH="
if exist "bin\Release\hvac_box_parametric.dll" (
    set "DLL_PATH=bin\Release\hvac_box_parametric.dll"
) else if exist "bin\hvac_box_parametric.dll" (
    set "DLL_PATH=bin\hvac_box_parametric.dll"
) else if exist "Release\hvac_box_parametric.dll" (
    set "DLL_PATH=Release\hvac_box_parametric.dll"
)

if not defined DLL_PATH (
    echo [WARN] Cannot locate dll file automatically.
    echo        Please find hvac_box_parametric.dll in the build folder
    echo        and copy it to: %UGII_BASE_DIR%\application\
    echo.
    cd /d ..
    echo 按任意键关闭...
    pause >nul
    goto :eof
)

echo Found: %DLL_PATH%
echo.

REM --- 自动复制到NX目录 ---
echo ============================================================
echo  是否自动复制到NX application目录?
echo  目标: %UGII_BASE_DIR%\application\
echo ============================================================
echo.
set /p COPY_CHOICE="输入 Y 确认复制, 其他键跳过: "

if /i "%COPY_CHOICE%"=="Y" (
    REM 创建application目录(如果不存在)
    if not exist "%UGII_BASE_DIR%\application" mkdir "%UGII_BASE_DIR%\application"

    copy /Y "%DLL_PATH%" "%UGII_BASE_DIR%\application\hvac_box_parametric.dll"
    if %errorlevel% neq 0 (
        echo.
        echo [ERROR] 复制dll失败! 可能需要管理员权限。
        echo 请手动复制:
        echo   从: %cd%\%DLL_PATH%
        echo   到: %UGII_BASE_DIR%\application\
        echo.
    ) else (
        echo [OK] dll copied.
    )

    copy /Y "..\config\param_defaults.json" "%UGII_BASE_DIR%\application\param_defaults.json"
    if %errorlevel% neq 0 (
        echo [WARN] param_defaults.json copy failed (non-critical).
    ) else (
        echo [OK] param_defaults.json copied.
    )

    echo.
    echo ==========================================
    echo  部署完成! 现在可以在NX2306中使用:
    echo  文件 - 执行 - NX Open - 选择dll
    echo ==========================================
) else (
    echo.
    echo 跳过复制。请手动复制以下文件到NX目录:
    echo   %cd%\%DLL_PATH%
    echo   ..\config\param_defaults.json
    echo 目标: %UGII_BASE_DIR%\application\
)

cd /d ..
echo.
echo 全部完成!
echo.
echo 按任意键关闭...
pause >nul
