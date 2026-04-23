@echo off
setlocal
cd /d "%~dp0.."
rexglue migrate --app_root .
pause
