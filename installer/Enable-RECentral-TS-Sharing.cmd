@echo off
cd /d "%~dp0"
echo Applying read-sharing hook to RECentral...
recentral_share_injector.exe
echo.
echo Run this after starting RECentral and before starting a new recording.
pause

