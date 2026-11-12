# The Windows zip, unpacked where nothing else is and run with nothing of GStreamer's on the path, as
# on a PC that has never had it: the app must start and find every part a game needs, and every
# plugin in the zip must load (2pstart --check-game says what it lacks, a line each).
#
# The runner has no graphics chip that decodes H.264, so the decoder is the one part it may lack
# here. Whether a real PC's decodes is seen on one.
#
#   ci/check-zip.ps1 <the zip>
param([Parameter(Mandatory = $true)][string]$Zip)
$ErrorActionPreference = 'Stop'

$folder = Join-Path $env:RUNNER_TEMP 'clean'
Remove-Item $folder -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -Path $Zip -DestinationPath $folder
$app = Get-ChildItem $folder -Recurse -Filter 2pstart.exe | Select-Object -First 1
if (-not $app) { throw "There is no 2pstart.exe in $Zip" }

# Windows' own folders only, and none of GStreamer's settings.
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\WindowsPowerShell\v1.0"
Get-ChildItem env: | Where-Object { $_.Name -like 'GST*' -or $_.Name -like 'GSTREAMER*' } |
    ForEach-Object { Remove-Item "env:$($_.Name)" }

$out = Join-Path $env:RUNNER_TEMP 'check-game.out'
$err = Join-Path $env:RUNNER_TEMP 'check-game.err'
$run = Start-Process -FilePath $app.FullName -ArgumentList '--check-game' -Wait -PassThru -NoNewWindow `
    -RedirectStandardOutput $out -RedirectStandardError $err
$said = @(Get-Content $out | Where-Object { $_ -ne '' })
$logged = @(Get-Content $err)
$said | ForEach-Object { "2pstart: $_" }
$logged | ForEach-Object { "  $_" }

# 0 when it can play; 1 when something is lacking; anything else is Windows refusing to start it,
# most often for a library that is not there.
if ($run.ExitCode -ne 0 -and $run.ExitCode -ne 1) {
    throw ("2pstart did not start: exit code {0} (0x{0:X8})" -f $run.ExitCode)
}
$wrong = @($said | Where-Object { $_ -ne 'lacks d3d11h264dec' })
if ($wrong.Count -gt 0) { throw "The zip lacks what a game needs: $($wrong -join '; ')" }
"The zip starts on its own and has every part a game needs, but the graphics chip's decoder."
