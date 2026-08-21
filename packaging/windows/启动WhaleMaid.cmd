@echo off
setlocal
title WhaleMaid Launcher
cd /d "%~dp0"
echo ============================================================
echo WhaleMaid Launcher
echo ============================================================
echo [1/2] Checking installation...
where powershell.exe >nul 2>nul
if errorlevel 1 goto :no_powershell
if not exist "%~dp0scripts\start-whalemaid.ps1" goto :missing_script
echo [2/2] Starting WhaleMaid. Detailed Chinese progress follows...
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\start-whalemaid.ps1"
set "WhaleMaidExitCode=%ERRORLEVEL%"
echo.
if not "%WhaleMaidExitCode%"=="0" goto :failed
echo SUCCESS - WhaleMaid is running.
set /p "WhaleMaidClosePrompt=Please close this window... "
exit /b 0

:no_powershell
echo ERROR: Windows PowerShell was not found.
set "WhaleMaidExitCode=10"
goto :failed

:missing_script
echo ERROR: scripts\start-whalemaid.ps1 is missing.
echo Please extract the complete ZIP before running this file.
set "WhaleMaidExitCode=11"
goto :failed

:failed
echo.
echo START FAILED. Error code: %WhaleMaidExitCode%
echo Read the error above or open the log shown by the launcher.
echo Please take a screenshot of this window if you need help.
set /p "WhaleMaidClosePrompt=Please close this window... "
exit /b %WhaleMaidExitCode%

