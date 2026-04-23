@echo off
setlocal

REM Go to parent of script directory
cd /d "%~dp0.."

REM Set REXSDK_DIR to your rexglue-sdk source tree.
REM Example: set REXSDK_DIR=C:\Users\you\dev\rexglue-sdk
if "%REXSDK_DIR%"=="" (
    echo ERROR: REXSDK_DIR is not set.
    echo Set it to the path of your rexglue-sdk source tree, e.g.:
    echo   set REXSDK_DIR=C:\Users\you\dev\rexglue-sdk
    pause
    exit /b 1
)

echo ================================
echo Step 1 Running rexglue migrate
echo ================================
rexglue migrate --app_root .
if errorlevel 1 (
    echo ERROR: migrate failed
    pause
    exit /b 1
)

echo ================================
echo Step 2 Configure
echo ================================
cmake --preset win-amd64-relwithdebinfo -DREXSDK_DIR="%REXSDK_DIR%" -DCMAKE_CXX_FLAGS="-march=x86-64-v3" -DCMAKE_C_FLAGS="-march=x86-64-v3"
if errorlevel 1 (
    echo ERROR: configure failed
    pause
    exit /b 1
)

echo ================================
echo Step 3 Run codegen
echo ================================
cmake --build --preset win-amd64-relwithdebinfo --target rekameo_codegen
if errorlevel 1 (
    echo ERROR: codegen failed
    pause
    exit /b 1
)

echo ================================
echo Step 4 Restore original files
echo ================================
if exist CMakeLists.txt.bak (
    copy /y CMakeLists.txt.bak CMakeLists.txt >nul
) else (
    echo WARNING: CMakeLists.txt.bak not found
)

if exist src\main.cpp.bak (
    copy /y src\main.cpp.bak src\main.cpp >nul
) else (
    echo WARNING: src\main.cpp.bak not found
)

echo ================================
echo Step 5 Reconfigure
echo ================================
cmake --preset win-amd64-relwithdebinfo -DREXSDK_DIR="%REXSDK_DIR%" -DCMAKE_CXX_FLAGS="-march=x86-64-v3" -DCMAKE_C_FLAGS="-march=x86-64-v3"
if errorlevel 1 (
    echo ERROR: reconfigure failed
    pause
    exit /b 1
)

echo ================================
echo Step 6 Final build
echo ================================
cmake --build --preset win-amd64-relwithdebinfo
if errorlevel 1 (
    echo ERROR: build failed
    pause
    exit /b 1
)

echo ================================
echo Build completed successfully
echo ================================
pause
