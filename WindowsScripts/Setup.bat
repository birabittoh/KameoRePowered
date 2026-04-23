@echo off
setlocal enabledelayedexpansion

REM Setup.bat - one-shot extraction of kameo.iso into assets/default.xex
REM
REM Expects:
REM   - kameo.iso placed at the repo root (legally dumped from your disc).
REM   - extract-xiso on PATH, or xextool.exe + extract-xiso.exe placed at
REM     tools\.
REM
REM Produces:
REM   - assets\default.xex (raw game executable ReXGlue codegens against)
REM
REM This script is intentionally conservative. It refuses to overwrite an
REM existing assets\default.xex unless you pass /force.

cd /d "%~dp0.."

set "ISO_PATH=%CD%\kameo.iso"
set "ASSETS=%CD%\assets"
set "EXTRACT_DIR=%CD%\out\iso_extract"
set "FORCE=%~1"

if not exist "%ISO_PATH%" (
    echo [Setup] Could not find kameo.iso at %ISO_PATH%.
    echo [Setup] Place your legally dumped Kameo ISO at the repo root and re-run.
    exit /b 1
)

if exist "%ASSETS%\default.xex" (
    if /i not "%FORCE%"=="/force" (
        echo [Setup] assets\default.xex already exists. Re-run with /force to overwrite.
        exit /b 0
    )
)

REM Look for extract-xiso on PATH, then tools\.
set "EXTRACT_XISO="
where extract-xiso >nul 2>&1 && set "EXTRACT_XISO=extract-xiso"
if "%EXTRACT_XISO%"=="" if exist "tools\extract-xiso.exe" set "EXTRACT_XISO=tools\extract-xiso.exe"

if "%EXTRACT_XISO%"=="" (
    echo [Setup] extract-xiso not found.
    echo [Setup] Install it ^(https://github.com/XboxDev/extract-xiso^)
    echo          or drop extract-xiso.exe into .\tools\.
    exit /b 1
)

if not exist "%EXTRACT_DIR%" mkdir "%EXTRACT_DIR%"
if not exist "%ASSETS%"      mkdir "%ASSETS%"

echo [Setup] Extracting kameo.iso with %EXTRACT_XISO% ...
"%EXTRACT_XISO%" -d "%EXTRACT_DIR%" "%ISO_PATH%"
if errorlevel 1 (
    echo [Setup] extract-xiso failed.
    exit /b 1
)

REM Kameo's default.xex lives at the root of the extracted image.
if not exist "%EXTRACT_DIR%\default.xex" (
    echo [Setup] default.xex not found after extraction.
    echo [Setup] Inspect %EXTRACT_DIR% and copy the game XEX into assets\default.xex manually.
    exit /b 1
)

copy /y "%EXTRACT_DIR%\default.xex" "%ASSETS%\default.xex" >nul
echo [Setup] assets\default.xex is ready.

endlocal
