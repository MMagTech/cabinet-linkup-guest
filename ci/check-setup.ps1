# The Windows Setup, as a friend runs it: installed for this person alone, then the app run from
# where it went with nothing of GStreamer's on the path, as on a PC that has never had it. The app
# must start and find every part a game needs, and every plugin must load (2P Start --check-game says
# what it lacks, a line each); every line of Credits must have its licence text. Then it is
# uninstalled, which must take the app, its Start menu entry, its Startup entry and its name for
# notifications, and keep the friend's pairing, as a silent uninstall asks nothing.
#
# The runner has no graphics chip that decodes H.264, so the decoder is the one part it may lack
# here. Whether a real PC's decodes is seen on one.
#
#   ci/check-setup.ps1 <the Setup>
param([Parameter(Mandatory = $true)][string]$Setup)
$ErrorActionPreference = 'Stop'

$installed = Join-Path $env:LOCALAPPDATA 'Programs\2P Start'
$app = Join-Path $installed '2P Start.exe'
$entry = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\2P Start.lnk'
$data = Join-Path $env:LOCALAPPDATA 'CabinetOS 2P Start'
$run = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$named = 'HKCU:\Software\Classes\AppUserModelId\io.github.mmagtech.2pstart'

$log = Join-Path $env:RUNNER_TEMP 'setup.log'
$p = Start-Process -FilePath (Resolve-Path $Setup).Path -Wait -PassThru `
    -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=$log"
if ($p.ExitCode -ne 0) {
    Get-Content $log -Tail 40
    throw "Setup ended with $($p.ExitCode)"
}
if (-not (Test-Path $app)) { throw "Setup put no 2P Start.exe in $installed" }
if (-not (Test-Path $entry)) { throw "Setup made no Start menu entry" }
$files = Get-ChildItem $installed -Recurse -File
"Installed: {0} files, {1:N1} MB; Setup {2:N1} MB" -f $files.Count, (($files | Measure-Object Length -Sum).Sum / 1MB), ((Get-Item $Setup).Length / 1MB)

# Windows' own folders only, and none of GStreamer's settings.
$path = $env:PATH
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\WindowsPowerShell\v1.0"
$gst = @(Get-ChildItem env: | Where-Object { $_.Name -like 'GST*' })
$gst | ForEach-Object { Remove-Item "env:$($_.Name)" }
$out = Join-Path $env:RUNNER_TEMP 'check-game.out'
$err = Join-Path $env:RUNNER_TEMP 'check-game.err'
$game = Start-Process -FilePath $app -ArgumentList '--check-game' -Wait -PassThru -NoNewWindow `
    -RedirectStandardOutput $out -RedirectStandardError $err
$env:PATH = $path
$gst | ForEach-Object { Set-Item "env:$($_.Name)" $_.Value }
$said = @(Get-Content $out | Where-Object { $_ -ne '' })
$said | ForEach-Object { "2P Start: $_" }
Get-Content $err | ForEach-Object { "  $_" }

# 0 when it can play; 1 when something is lacking; anything else is Windows refusing to start it,
# most often for a library that is not there.
if ($game.ExitCode -ne 0 -and $game.ExitCode -ne 1) {
    throw ("2P Start did not start: exit code {0} (0x{0:X8})" -f $game.ExitCode)
}
$licences = Join-Path $installed 'licenses'
foreach ($name in 'GStreamer', 'GLib', 'SDL', 'OpenSSL', 'curl') {
    if (-not (Get-ChildItem (Join-Path $licences $name) -File -ErrorAction SilentlyContinue)) { throw "No licence text for $name" }
}
$wrong = @($said | Where-Object { $_ -ne 'lacks d3d11h264dec' })
if ($wrong.Count -gt 0) { throw "The app lacks what a game needs: $($wrong -join '; ')" }
"The installed app starts on its own and has every part a game needs, but the graphics chip's decoder."

# What the app itself leaves, as it would after a friend's use: a pairing, Startup on, and the name.
New-Item -ItemType Directory -Force $data | Out-Null
Set-Content (Join-Path $data 'settings.json') '{}'
# A new person's Run key may not be there yet; the app makes it as this does.
if (-not (Test-Path $run)) { New-Item -Path $run | Out-Null }
New-ItemProperty -Path $run -Name 'CabinetOS 2P Start' -Value "`"$app`" --startup" -Force | Out-Null
New-Item -Path $named -Force | Out-Null

# The uninstaller runs a copy of itself and returns at once: what it took is waited for.
$uninstaller = Join-Path $installed 'unins000.exe'
Start-Process -FilePath $uninstaller -Wait -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART'
$waited = 0
while ((Test-Path $installed) -and $waited -lt 60) {
    Start-Sleep -Seconds 1
    $waited++
}
if (Test-Path $installed) { throw "The uninstaller left $installed" }
if (Test-Path $entry) { throw "The uninstaller left the Start menu entry" }
if ((Get-ItemProperty -Path $run -ErrorAction SilentlyContinue).'CabinetOS 2P Start') { throw "The uninstaller left Startup's entry" }
if (Test-Path $named) { throw "The uninstaller left the name for notifications" }
if (-not (Test-Path (Join-Path $data 'settings.json'))) { throw "A silent uninstall took the friend's pairing" }
Remove-Item $data -Recurse -Force
"Uninstalled: the app, its Start menu entry, Startup and its name gone; the friend's pairing kept."
