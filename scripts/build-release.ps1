$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$CMake = 'C:\Program Files\CMake\bin\cmake.exe'
$IsccCandidates = @(
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
    'C:\Program Files\Inno Setup 6\ISCC.exe'
)
$Iscc = $IsccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not (Test-Path $CMake)) { throw 'CMake was not found.' }
if (-not $Iscc) { throw 'Inno Setup 6 was not found.' }

Push-Location "$Root\src\recentral-share-hook"
try {
    & cmd.exe /d /c build.cmd
    if ($LASTEXITCODE -ne 0) { throw 'Sharing hook build failed.' }
} finally { Pop-Location }

Push-Location "$Root\src\obs-plugin"
try {
    & $CMake --preset windows-x64
    if ($LASTEXITCODE -ne 0) { throw 'OBS plugin configure failed.' }
    & $CMake --build --preset windows-x64 --config RelWithDebInfo --parallel
    if ($LASTEXITCODE -ne 0) { throw 'OBS plugin build failed.' }
} finally { Pop-Location }

$ObsBin = "$Root\dist\obs-plugin\bin\64bit"
$ObsData = "$Root\dist\obs-plugin\data\locale"
$HookBin = "$Root\dist\hook"
New-Item -ItemType Directory -Force -Path $ObsBin, $ObsData, $HookBin | Out-Null

Copy-Item "$Root\src\obs-plugin\build_x64\rundir\RelWithDebInfo\c875-follow-source.dll" $ObsBin -Force
Copy-Item "$Root\src\obs-plugin\data\locale\*.ini" $ObsData -Force
Copy-Item "$Root\src\recentral-share-hook\build\recentral_share_hook.dll" $HookBin -Force
Copy-Item "$Root\src\recentral-share-hook\build\recentral_share_injector.exe" $HookBin -Force

Push-Location "$Root\installer"
try {
    & $Iscc 'AVT-C875-Follow-Source.iss'
    if ($LASTEXITCODE -ne 0) { throw 'Installer build failed.' }
} finally { Pop-Location }

Write-Host "Installer: $Root\installer\output\AVT-C875-Follow-Source-Setup.exe"
