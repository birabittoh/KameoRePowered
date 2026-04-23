@echo off
setlocal

cd /d "%~dp0.."

echo Copying rekameo.exe and DLLs to the repo root...
copy /y "out\build\win-amd64-relwithdebinfo\rekameo.exe" .
copy /y "out\build\win-amd64-relwithdebinfo\*.dll" .

if not exist rekameo.exe (
    echo ERROR: Failed to copy rekameo.exe to %cd%
    pause
    exit /b 1
)

echo Starting rekameo.exe with default arguments...
rekameo.exe --gpu_allow_invalid_fetch_constants=true --enable_console=false --scribble_heap=true --vsync=off --video_mode_refresh_rate=60
