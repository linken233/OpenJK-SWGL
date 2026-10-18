# Runs SWGL_SP in a sandbox copy of the user's game folder, so their config and saves are never touched.
# Usage: run_sandbox.ps1 [-Save a] [-Cvars "r_drawfog 2"] [-WaitFrames 900] [-Fresh]
# Output: sandbox\home\SWGL\qconsole.log and sandbox\home\SWGL\screenshots\*.jpg|tga
param(
	[string]$Save = "a",
	[string]$Cvars = "",
	[string]$Cmds = "",
	[int]$WaitFrames = 900,
	[int]$TimeoutSec = 180,
	[switch]$Fresh
)
$ErrorActionPreference = "Stop"
$src  = "C:\Program Files (x86)\Steam\steamapps\common\Jedi Academy - SWGL_DEV\GameData"
$home0 = "$env:USERPROFILE\Documents\My Games\SWGL"
$root = Join-Path $PSScriptRoot "..\..\sandbox" | Resolve-Path -ErrorAction SilentlyContinue
if (-not $root) { $root = Join-Path (Resolve-Path "$PSScriptRoot\..\..").Path "sandbox"; New-Item -ItemType Directory $root | Out-Null }
$game = Join-Path $root "game"
$home1 = Join-Path $root "home"

if ($Fresh -and (Test-Path $home1)) { Remove-Item $home1 -Recurse -Force }
# the game files are always refreshed (new DLL builds); the home (cfg, saves) only the first time
robocopy $src $game /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
if (-not (Test-Path $home1)) { robocopy $home0 $home1 /E /XF qconsole.log crashlog* /NFL /NDL /NJH /NJS /NP | Out-Null }
Remove-Item "$home1\SWGL\qconsole.log" -ErrorAction SilentlyContinue
Remove-Item "$home1\SWGL\screenshots\*" -ErrorAction SilentlyContinue

$args1 = @("+set","fs_homepath",$home1,"+set","logfile","2","+set","developer","1")
foreach ($c in ($Cvars -split ";" | Where-Object { $_ })) { $args1 += @("+set") + ($c.Trim() -split " ",2) }
$args1 += @("+load",$Save,"+wait",$WaitFrames)
# commands run in game once the save is loaded, e.g. "cg_thirdperson 1;toggleconsole"
foreach ($c in ($Cmds -split ";" | Where-Object { $_ })) { $args1 += @("+" + $c.Trim().Split(" ")[0]) + ($c.Trim().Split(" ") | Select-Object -Skip 1) }
if ($Cmds) { $args1 += @("+wait","60") }
$args1 += @("+screenshot","+wait","30","+quit")
$p = Start-Process -FilePath "$game\SWGL_SP.x86_64.exe" -WorkingDirectory $game -ArgumentList $args1 -PassThru
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Write-Host "timeout, killing the SANDBOX process $($p.Id)"; $p.Kill() }
Write-Host "exit code $($p.ExitCode)"
Get-ChildItem "$home1\SWGL\screenshots" -ErrorAction SilentlyContinue | Select-Object FullName
