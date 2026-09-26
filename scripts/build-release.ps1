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

$ReleaseDir = "$Root\release"
$ManualStage = "$Root\installer\output\manual-stage"
$ManualZip = "$Root\installer\output\AVT-C875-Follow-Source-manual.zip"
New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null
if (Test-Path $ManualStage) { Remove-Item -LiteralPath $ManualStage -Recurse -Force }
New-Item -ItemType Directory -Force -Path "$ManualStage\obs-plugin\bin\64bit", "$ManualStage\obs-plugin\data\locale", "$ManualStage\hook" | Out-Null
Copy-Item "$ObsBin\c875-follow-source.dll" "$ManualStage\obs-plugin\bin\64bit" -Force
Copy-Item "$ObsData\*.ini" "$ManualStage\obs-plugin\data\locale" -Force
Copy-Item "$HookBin\recentral_share_hook.dll", "$HookBin\recentral_share_injector.exe" "$ManualStage\obs-plugin\data" -Force
Copy-Item "$HookBin\recentral_share_hook.dll", "$HookBin\recentral_share_injector.exe" "$ManualStage\hook" -Force
Copy-Item "$Root\installer\Enable-RECentral-TS-Sharing.cmd" "$ManualStage\hook" -Force
Copy-Item "$Root\README.md", "$Root\LICENSE", "$Root\THIRD_PARTY_NOTICES.md" $ManualStage -Force
if (Test-Path $ManualZip) { Remove-Item -LiteralPath $ManualZip -Force }
Compress-Archive -Path "$ManualStage\*" -DestinationPath $ManualZip -CompressionLevel Optimal
Remove-Item -LiteralPath $ManualStage -Recurse -Force

$Installer = "$Root\installer\output\AVT-C875-Follow-Source-Setup.exe"
Copy-Item $Installer, $ManualZip $ReleaseDir -Force
Write-Host "Installer: $Installer"
Write-Host "Manual ZIP: $ManualZip"
