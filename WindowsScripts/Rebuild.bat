@echo off
setlocal

REM Set REXSDK_DIR to your rexglue-sdk source tree.
REM Example: set REXSDK_DIR=C:\Users\you\dev\rexglue-sdk
if "%REXSDK_DIR%"=="" (
    echo ERROR: REXSDK_DIR is not set.
    echo Set it to the path of your rexglue-sdk source tree, e.g.:
    echo   set REXSDK_DIR=C:\Users\you\dev\rexglue-sdk
    pause
    exit /b 1
)

cd /d "%~dp0.."
cmake --preset win-amd64-relwithdebinfo -DREXSDK_DIR="%REXSDK_DIR%" -DCMAKE_CXX_FLAGS="-march=x86-64-v3" -DCMAKE_C_FLAGS="-march=x86-64-v3"
cmake --build --preset win-amd64-relwithdebinfo
pause
