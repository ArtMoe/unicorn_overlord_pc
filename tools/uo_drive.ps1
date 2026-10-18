# Scripted UO session: boots the game, runs timed steps, dumps frames, stops it.
# usage: uo_drive.ps1 -Name <name> -Steps "30:dump","32:cross","33:release" [-Seconds 60] [-Launch run|setup]
# step actions: dump (frame dump), release (empty pad file), anything else = pad tokens
param([string]$Name = 'session', [string[]]$Steps = @(), [int]$Seconds = 60, [string]$Launch = 'run')
$port = 'D:\Emulation\Recomps\unicorn_overlord_pc'
$run = "$port\run_uo"; $dump = "$run\dump\$Name"
New-Item -ItemType Directory -Force $dump | Out-Null
Set-Content -Path "$run\pad.txt" -Value '' -NoNewline
if ($Launch -eq 'run') { $env:VW_GAME_DIR = 'D:\Emulation\Games\ps4\ShadPs4Plus-PkgExtractor-1.1-windows\CUSA36381' }
$env:VW_DATA_DIR = $run
$env:VW_PAD_FILE = "$run\pad.txt"; $env:VW_FRAME_DUMP_TRIGGER = "$run\dump_now"; $env:VW_DUMP_DIR = $dump
$script = if ($Launch -eq 'setup') { "$port\launcher\unicorn_launcher.py" } else { "$port\run.py" }
$p = Start-Process python -ArgumentList "`"$script`"" -RedirectStandardOutput "$run\$Name.out" `
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
Get-Process vwport -ErrorAction SilentlyContinue | Stop-Process -Force
if (-not $p.HasExited) { $p | Stop-Process -Force -ErrorAction SilentlyContinue }
if ($exited) { "game exited by itself (code $($p.ExitCode))" } else { "stopped after $Seconds s" }
Get-ChildItem $dump -Filter *.raw | ForEach-Object { $_.Name }
