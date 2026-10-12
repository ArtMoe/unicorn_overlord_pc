# Scripted test session: boots the game, runs timed steps, dumps frames, stops it.
# usage: drive.ps1 -Name <log name> -Steps "15:dump","16:cross","17:release",... [-Seconds 60]
# step actions: dump (frame dump), release (empty pad file), anything else = pad tokens
param([string]$Name = 'session', [string[]]$Steps = @(), [int]$Seconds = 60)
$r = 'D:\Emulation\Recomps\Odin Sphere Leifthrasir'
$run = "$r\run"; $dump = "$run\dump\$Name"
New-Item -ItemType Directory -Force $dump | Out-Null
Set-Content -Path "$run\pad.txt" -Value '' -NoNewline
$env:GOW3_GAME_DIR = "$r\game_dump\CUSA05083"; $env:GOW3_DATA_DIR = $run
$env:GOW3_PAD_FILE = "$run\pad.txt"; $env:GOW3_FRAME_DUMP_TRIGGER = "$run\dump_now"; $env:GOW3_DUMP_DIR = $dump
if (-not $env:GOW3_HANG_DUMP) { $env:GOW3_HANG_DUMP = '0' }
$p = Start-Process python -ArgumentList "`"$r\port\run.py`"" -RedirectStandardOutput "$run\$Name.out" `
    -RedirectStandardError "$run\$Name.err" -PassThru -NoNewWindow
$start = [DateTime]::Now
foreach ($step in $Steps) {
    $at, $action = $step.Split(':', 2)
    $wait = [int]([double]$at * 1000 - ([DateTime]::Now - $start).TotalMilliseconds)
    if ($wait -gt 0 -and $p.WaitForExit($wait)) { break }
    if ($p.HasExited) { break }
    switch ($action) {
        'dump'    { New-Item -ItemType File "$run\dump_now" -Force | Out-Null }
        'release' { Set-Content -Path "$run\pad.txt" -Value '' -NoNewline }
        default   { Set-Content -Path "$run\pad.txt" -Value $action -NoNewline }
    }
}
$left = [int]($Seconds * 1000 - ([DateTime]::Now - $start).TotalMilliseconds)
if ($left -gt 0) { [void]$p.WaitForExit($left) }
$exited = $p.HasExited
Get-Process gow3-probe -ErrorAction SilentlyContinue | Stop-Process -Force
if (-not $p.HasExited) { $p | Stop-Process -Force -ErrorAction SilentlyContinue }
if ($exited) { "game exited by itself (code $($p.ExitCode))" } else { "stopped after $Seconds s" }
