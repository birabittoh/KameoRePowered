@echo off
setlocal

cd /d "%~dp0.."

echo Copying rekameo.exe, PDB, and DLLs to the repo root...
copy /y "out\build\win-amd64-relwithdebinfo\rekameo.exe" .
copy /y "out\build\win-amd64-relwithdebinfo\rekameo.pdb" .
copy /y "out\build\win-amd64-relwithdebinfo\*.dll" .

if not exist rekameo.exe (
    echo ERROR: Failed to copy rekameo.exe to %cd%
    pause
    exit /b 1
)

echo Opening rekameo.exe in Visual Studio for debugging...
REM Adjust the devenv path if your install is elsewhere.
start "" "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" /debugexe rekameo.exe --gpu_allow_invalid_fetch_constants=true --enable_console=true --scribble_heap=true --vsync=off --video_mode_refresh_rate=60
