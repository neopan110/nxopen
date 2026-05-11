@echo off
chcp 65001 >nul 2>&1
title HVAC Box Parametric - Build Script

echo.
echo ==========================================
echo   HVAC Box Parametric Builder v1.0
echo ==========================================
echo.

REM --- Check NX environment ---
if not defined UGII_BASE_DIR (
    echo [ERROR] UGII_BASE_DIR not set!
    echo.
    echo Please set environment variable first:
    echo   Variable: UGII_BASE_DIR
    echo   Value:    C:\Program Files\Siemens\NX2306
    echo.
    echo Then reopen cmd and run build.bat again.
    echo.
    pause
    goto :eof
)

echo [OK] NX Base Dir: %UGII_BASE_DIR%

if not exist "%UGII_BASE_DIR%\UGOPEN" (
    echo [ERROR] UGOPEN folder not found at: %UGII_BASE_DIR%\UGOPEN
    echo Please check your UGII_BASE_DIR path.
    pause
    goto :eof
)

echo [OK] UGOPEN exists.

REM --- Find VS2019 ---
set "VS_PATH="
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not defined VS_PATH if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat"
if not defined VS_PATH if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"

if not defined VS_PATH (
    echo [ERROR] Visual Studio 2019 not found!
    echo Please install VS2019 Community Edition.
    echo Do NOT use VS2022.
    pause
    goto :eof
)

echo [OK] VS2019 found.

REM --- Init VS environment ---
echo [INFO] Initializing VS2019 x64...
call "%VS_PATH%" x64 >nul 2>&1

REM --- Check cmake ---
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found!
    echo Install CMake or add VS2019 CMake component.
    pause
    goto :eof
)
echo [OK] CMake found.

REM --- Build ---
echo.
echo [INFO] Creating build directory...
if not exist build mkdir build
pushd build

echo [INFO] CMake configure...
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DNX_VERSION=2306000

if %errorlevel% neq 0 (
    echo [ERROR] CMake configure FAILED!
    popd
    pause
    goto :eof
)

echo [INFO] Compiling (Release)...
cmake --build . --config Release -- /m

if %errorlevel% neq 0 (
    echo [ERROR] Compile FAILED!
    popd
    pause
    goto :eof
)

echo.
echo ==========================================
echo   BUILD SUCCESSFUL!
echo ==========================================
echo.

REM --- Find dll ---
set "DLL_PATH="
if exist "bin\Release\hvac_box_parametric.dll" set "DLL_PATH=bin\Release\hvac_box_parametric.dll"
if not defined DLL_PATH if exist "bin\hvac_box_parametric.dll" set "DLL_PATH=bin\hvac_box_parametric.dll"
if not defined DLL_PATH if exist "Release\hvac_box_parametric.dll" set "DLL_PATH=Release\hvac_box_parametric.dll"

if not defined DLL_PATH (
    echo [WARN] Cannot find dll. Please locate it manually in build folder.
    popd
    pause
    goto :eof
)

echo Output: %cd%\%DLL_PATH%
echo.

REM --- Copy to NX ---
set /p COPY_CHOICE="Copy to NX application folder? (Y/N): "
if /i "%COPY_CHOICE%"=="Y" (
    if not exist "%UGII_BASE_DIR%\application" mkdir "%UGII_BASE_DIR%\application"
    copy /Y "%DLL_PATH%" "%UGII_BASE_DIR%\application\hvac_box_parametric.dll"
    copy /Y "..\config\param_defaults.json" "%UGII_BASE_DIR%\application\param_defaults.json"
    echo [OK] Files copied. Ready to use in NX2306.
)

popd
echo.
echo Done! Press any key to close.
pause >nul
