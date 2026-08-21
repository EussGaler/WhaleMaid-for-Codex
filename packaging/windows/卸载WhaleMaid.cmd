@echo off
setlocal
title WhaleMaid Uninstaller
cd /d "%~dp0"
echo ============================================================
echo WhaleMaid Uninstaller
echo ============================================================
choice /C YN /N /M "Remove WhaleMaid? [Y/N] "
if errorlevel 2 exit /b 0
where powershell.exe >nul 2>nul
if errorlevel 1 goto :no_powershell
if not exist "%~dp0scripts\uninstall-whalemaid.ps1" goto :missing_script
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\uninstall-whalemaid.ps1"
set "WhaleMaidExitCode=%ERRORLEVEL%"
echo.
if not "%WhaleMaidExitCode%"=="0" goto :failed
echo SUCCESS - WhaleMaid was removed.
set /p "WhaleMaidClosePrompt=Please close this window... "
exit /b 0

:no_powershell
echo ERROR: Windows PowerShell was not found.
set "WhaleMaidExitCode=10"
goto :failed

:missing_script
echo ERROR: scripts\uninstall-whalemaid.ps1 is missing.
set "WhaleMaidExitCode=11"
goto :failed

:failed
echo.
echo UNINSTALL FAILED. Error code: %WhaleMaidExitCode%
echo Read the error above or open the log shown by the uninstaller.
set /p "WhaleMaidClosePrompt=Please close this window... "
exit /b %WhaleMaidExitCode%

