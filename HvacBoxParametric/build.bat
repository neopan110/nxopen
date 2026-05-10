@echo off
REM ================================================================
REM  HVAC Box Parametric - 一键编译脚本
REM  使用方法: 双击本文件即可编译
REM  前提条件: 已安装VS2019 + NX2306环境变量已设置
REM ================================================================

echo.
echo ==========================================
echo   HVAC Box Parametric Builder v1.0
echo   One-Click Build Script
echo ==========================================
echo.

REM --- 检查NX环境 ---
if not defined UGII_BASE_DIR (
    echo [ERROR] UGII_BASE_DIR not set!
    echo.
    echo Please set NX2306 path first:
    echo   set UGII_BASE_DIR=C:\Siemens\NX2306
    echo.
    echo Or run this from "NX2306 Command Prompt"
    echo.
    pause
    exit /b 1
)

echo [OK] NX Base Dir: %UGII_BASE_DIR%

REM --- 检查VS2019 ---
set VS_PATH=
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    set VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    set VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat
)

if "%VS_PATH%"=="" (
    echo [ERROR] Visual Studio 2019 not found!
    echo.
    echo Please install VS2019 Community Edition:
    echo   https://visualstudio.microsoft.com/vs/older-downloads/
    echo.
    pause
    exit /b 1
)

echo [OK] VS2019 found.

REM --- 初始化VS环境 ---
echo.
echo [INFO] Initializing VS2019 x64 environment...
call "%VS_PATH%" x64 >nul 2>&1

REM --- 检查cmake ---
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found in PATH!
    echo.
    echo Please install CMake from: https://cmake.org/download/
    echo Or ensure VS2019 CMake component is installed.
    echo.
    pause
    exit /b 1
)
echo [OK] CMake found.

REM --- 创建build目录 ---
echo.
echo [INFO] Creating build directory...
if not exist build mkdir build
cd build

REM --- CMake配置 ---
echo.
echo [INFO] Running CMake configure...
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DNX_VERSION=2306000

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] CMake configure FAILED!
    echo Check error messages above.
    echo.
    cd ..
    pause
    exit /b 1
)

echo [OK] CMake configure successful.

REM --- 编译 ---
echo.
echo [INFO] Building (Release)...
cmake --build . --config Release -- /m

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build FAILED!
    echo Check error messages above.
    echo.
    cd ..
    pause
    exit /b 1
)

echo.
echo ==========================================
echo   BUILD SUCCESSFUL!
echo ==========================================
echo.
echo Output: %cd%\bin\hvac_box_parametric.dll
echo.
echo Next steps:
echo   1. Copy hvac_box_parametric.dll to:
echo      %UGII_BASE_DIR%\application\
echo.
echo   2. Copy ..\config\param_defaults.json to:
echo      %UGII_BASE_DIR%\application\
echo.
echo   3. In NX2306:
echo      File - Execute - NX Open - select dll
echo.

REM --- 自动复制到NX目录(可选) ---
set /p COPY_CHOICE="Auto-copy to NX application folder? (Y/N): "
if /i "%COPY_CHOICE%"=="Y" (
    copy /Y "bin\hvac_box_parametric.dll" "%UGII_BASE_DIR%\application\" >nul
    copy /Y "..\config\param_defaults.json" "%UGII_BASE_DIR%\application\" >nul
    echo [OK] Files copied to NX application folder.
)

cd ..
echo.
echo Done!
pause
