param(
    [double]$TargetLatencySeconds = 5,
    [string]$CaptureDirectory = "$env:USERPROFILE\Videos\Captures",
    [string]$InputFile = ""
)

$ErrorActionPreference = 'Stop'

if (-not $InputFile) {
    $latest = Get-ChildItem -LiteralPath $CaptureDirectory -Filter '*.ts' -File |
        Sort-Object CreationTime -Descending |
        Select-Object -First 1

    if (-not $latest) {
        throw "TS file not found in: $CaptureDirectory"
    }
    $InputFile = $latest.FullName
}

$ffprobe = Get-Command ffprobe -ErrorAction Stop
$ffplay = Get-Command ffplay -ErrorAction Stop
$item = Get-Item -LiteralPath $InputFile

$durationText = & $ffprobe.Source -v error `
    -show_entries format=duration `
    -of default=noprint_wrappers=1:nokey=1 `
    $item.FullName

if ($LASTEXITCODE -ne 0 -or -not $durationText) {
    throw "ffprobe could not read the recording: $($item.FullName)"
}

$duration = [double]::Parse(
    ($durationText | Select-Object -First 1),
    [Globalization.CultureInfo]::InvariantCulture)
$seek = [Math]::Max(0, $duration - $TargetLatencySeconds)
$age = ((Get-Date) - $item.LastWriteTime).TotalSeconds

Write-Host "Input          : $($item.FullName)"
Write-Host "Current duration: $([Math]::Round($duration, 3)) sec"
Write-Host "Start position : $([Math]::Round($seek, 3)) sec"
Write-Host "Target latency : $TargetLatencySeconds sec"
Write-Host "Last write age : $([Math]::Round($age, 2)) sec"

if ($age -gt 10) {
    Write-Warning 'The newest TS does not appear to be growing right now.'
}

& $ffplay.Source `
    -hide_banner `
    -loglevel info `
    -window_title 'C875 Follow Preview' `
    -sync audio `
    -ss $seek `
    $item.FullName

exit $LASTEXITCODE

