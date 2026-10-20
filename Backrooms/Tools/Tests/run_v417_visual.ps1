<#
v4.17 (visuel) : lance le banc -BRAutoTest -BRAutoTestV417Visual une fois par profil et archive Saved\AutoTest.

Chaque lancement : UnrealEditor.exe <projet> -game -windowed -ResX -ResY -unattended -nosplash -log -BRAutoTest
  -BRAutoTestV417Visual -BRProfile=<p> -BRCommit=<sha> -abslog=<dossier>\game.log
puis copie de Saved\AutoTest (captures, fiches <capture>.json, Captures.csv, Rapport.txt/json) dans
<OutRoot>\<Tag>\<profil>\, avec commit.txt (commit=<sha>, modifie=0|1). Le commit est celui de HEAD ; un arbre modifie
est signale (les captures portent alors le commit de base, pas le code lance).

Exemple (PowerShell 5.1, depuis n'importe quel dossier) :
  powershell -ExecutionPolicy Bypass -File Tools\Tests\run_v417_visual.ps1 -Tag avant -Profiles 1,2,0
Aucune autre instance d'Unreal ne doit tourner : un lancement est saute sinon.
#>
param(
  [string]$Tag = '',
  # Profils separes par des virgules ("1,2,0") : une chaine, car -File ne convertit pas une liste en tableau
  [string]$Profiles = '1',
  [int]$ResX = 1366,
  [int]$ResY = 768,
  [string]$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe',
  [string]$OutRoot = '',
  [int]$TimeoutMin = 40,
  [string]$Extra = ''
)
$ErrorActionPreference = 'Continue'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Backrooms.uproject')).Path
$projDir = Split-Path -Parent $project
$saved = Join-Path $projDir 'Saved'
if (-not $OutRoot) { $OutRoot = Join-Path $saved 'TestRuns\v417' }
if (-not $Tag) { $Tag = 'v417_' + (Get-Date -Format 'yyyyMMdd_HHmm') }
$names = @{ 0 = 'Performance'; 1 = 'Qualite'; 2 = 'Cinematique'; 3 = 'Personnalise'; 4 = 'RTXFluide' }
$sha = (git -C $projDir rev-parse HEAD).Trim()
$dirty = if ((git -C $projDir status --porcelain --untracked-files=no -- Source Content Config) -ne $null) { 1 } else { 0 }
$tagDir = Join-Path $OutRoot $Tag
New-Item -ItemType Directory -Force $tagDir | Out-Null
$profileList = @($Profiles.Split(',') | ForEach-Object { [int]$_.Trim() })
foreach ($p in $profileList) {
  if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { "profil $p SAUTE : un autre Unreal tourne"; continue }
  $dir = Join-Path $tagDir $names[$p]
  if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
  New-Item -ItemType Directory -Force $dir | Out-Null
  if (Test-Path "$saved\AutoTest") { Remove-Item -Recurse -Force "$saved\AutoTest" }
  $log = Join-Path $dir 'game.log'
  $args = @("`"$project`"", '-game', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-unattended', '-nosplash', '-log',
    '-BRAutoTest', '-BRAutoTestV417Visual', "-BRProfile=$p", "-BRCommit=$sha", "-abslog=`"$log`"")
  if ($Extra) { $args += $Extra.Split(' ') }
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $proc = Start-Process -FilePath $Editor -ArgumentList $args -PassThru
  $null = $proc.Handle
  $done = $proc.WaitForExit($TimeoutMin * 60 * 1000)
  if (-not $done) { Stop-Process -Id $proc.Id -Force; $proc.WaitForExit(); $code = 'TIMEOUT' } else { $code = $proc.ExitCode }
  $sw.Stop()
  if (Test-Path "$saved\AutoTest") { Copy-Item -Recurse -Force "$saved\AutoTest\*" $dir }
  Set-Content -Path (Join-Path $dir 'commit.txt') -Value "commit=$sha`nmodifie=$dirty`nprofil=$p" -Encoding ASCII
  $res = 'PAS DE RAPPORT'
  if (Test-Path "$dir\Rapport.json") {
    $j = Get-Content "$dir\Rapport.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    $res = "etat=$($j.etat) problemes=$(@($j.problemes).Count) non_verifies=$(@($j.non_verifies).Count)"
  }
  $shots = @(Get-ChildItem -Path $dir -Filter 'v417_*.png' -ErrorAction SilentlyContinue).Count
  $line = "{0,-12} exit={1} time={2:n1}min captures={3} {4}" -f $names[$p], $code, $sw.Elapsed.TotalMinutes, $shots, $res
  $line
  Add-Content -Path (Join-Path $tagDir 'summary.txt') -Value $line
}
