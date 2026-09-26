@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "Start-Process -FilePath 'cmd.exe' -Verb RunAs -ArgumentList '/k',('""%~dp0build\recentral_share_injector.exe""')"
