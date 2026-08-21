@echo off
setlocal
title WhaleMaid Installer
cd /d "%~dp0"
echo ============================================================
echo WhaleMaid Installer
echo ============================================================
echo [1/3] Launcher started.
where powershell.exe >nul 2>nul
if errorlevel 1 goto :no_powershell
if not exist "%~dp0scripts\install-whalemaid.ps1" goto :missing_script
echo [2/3] Running installer. Detailed Chinese progress follows...
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install-whalemaid.ps1"
set "WhaleMaidExitCode=%ERRORLEVEL%"
echo.
if not "%WhaleMaidExitCode%"=="0" goto :failed
echo [3/3] SUCCESS - WhaleMaid installation completed.
echo.
echo This window will stay open so you can read the result.
set /p "WhaleMaidClosePrompt=Please close this window... "
exit /b 0

:no_powershell
echo.
echo ERROR: Windows PowerShell was not found.
set "WhaleMaidExitCode=10"
goto :failed

:missing_script
echo.
echo ERROR: scripts\install-whalemaid.ps1 is missing.
echo Please extract the complete ZIP before running this file.
set "WhaleMaidExitCode=11"
goto :failed

:failed
echo.
echo INSTALLATION FAILED. Error code: %WhaleMaidExitCode%
echo Read the error above or open the log shown by the installer.
echo Please take a screenshot of this window if you need help.
set /p "WhaleMaidClosePrompt=Please close this window... "
exit /b %WhaleMaidExitCode%

