<#
Tests du jeu sans surveillance (v4.17, passe « appuis » : copie versionnee du lanceur qui ne vivait que dans
Saved/TestRuns/). Un lancement de l'editeur en mode jeu par jeu de verifications ; rapport Saved/AutoTest archive a chaque
fois. Resultats : Saved/TestRuns/autotest/<Tag>/<suite>/ (Rapport.txt/json, Mesures.csv, captures, game.log) et
Saved/TestRuns/autotest/<Tag>/summary.txt.

  powershell -ExecutionPolicy Bypass -File Tools\Tests\run_autotests.ps1 -Only "smoke,v415,v416vis,chapitre" -Tag essai
  depuis PowerShell : & Tools\Tests\run_autotests.ps1 -Only smoke,v48 -Tag essai

Le projet et Saved/ sont trouves depuis l'emplacement du script ; moteur : -Editor, sinon UE 5.8 a son chemin habituel.
Aucune autre instance d'Unreal ne doit tourner (la suite est sautee sinon). Apres coup : git checkout --
Config/DefaultEngine.ini (l'editeur y ecrit une section AndroidFileServer, a ne jamais commiter).
#>
param(
  [string[]]$Only,
  [string]$Tag,
  [string]$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
)
$ErrorActionPreference = 'Continue'
$proj = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Backrooms.uproject')).Path
$saved = Join-Path (Split-Path -Parent $proj) 'Saved'
$common = @('-game', '-windowed', '-ResX=1920', '-ResY=1080', '-unattended', '-nosplash', '-log')
$runs = [ordered]@{
  'smoke' = @{ a = @('-BRAutoTest', '-BRSmokeTest', '-BRAutoTestStrict'); t = 45 }
  'v415'  = @{ a = @('-BRAutoTest', '-BRAutoTestV415', '-BRAutoTestStrict'); t = 60 }
  'v415fix'= @{ a = @('-BRAutoTest', '-BRAutoTestV415Fix', '-BRAutoTestStrict'); t = 70 }
  'v416lidar' = @{ a = @('-BRAutoTest', '-BRAutoTestV416Lidar', '-BRAutoTestStrict'); t = 30 }
  'v416light' = @{ a = @('-BRAutoTest', '-BRAutoTestV416Light', '-BRAutoTestStrict'); t = 40 }
  'v416ladder' = @{ a = @('-BRAutoTest', '-BRAutoTestV416Ladder', '-BRAutoTestStrict'); t = 40 }
  'v416vis' = @{ a = @('-BRAutoTest', '-BRAutoTestV416Visual', '-BRAutoTestStrict'); t = 60 }
  'v416ship' = @{ a = @('-BRAutoTest', '-BRAutoTestV416Ship', '-BRAutoTestStrict'); t = 40 }
  'chapitre' = @{ a = @('-BRAutoTest', '-BRAutoTestChapter', '-BRAutoTestStrict'); t = 75 }
  'chapitre1' = @{ a = @('-BRAutoTest', '-BRAutoTestChapter', '-BRChapterPhase=1', '-BRAutoTestStrict'); t = 45 }
  'chapitre2' = @{ a = @('-BRAutoTest', '-BRAutoTestChapter', '-BRChapterPhase=2', '-BRAutoTestStrict'); t = 45 }
  'chapitre3' = @{ a = @('-BRAutoTest', '-BRAutoTestChapter', '-BRChapterPhase=3', '-BRAutoTestStrict'); t = 25 }
  'visual'= @{ a = @('-BRAutoTest', '-BRAutoTestVisual'); t = 60 }
  'visualhl1'= @{ a = @('-BRAutoTest', '-BRAutoTestVisual', '-BRHitLightingMode=1'); t = 60 }
  'visualc'= @{ a = @('-BRAutoTest', '-BRAutoTestVisual', '-BRProfile=2'); t = 40 }
  'visualchl1'= @{ a = @('-BRAutoTest', '-BRAutoTestVisual', '-BRProfile=2', '-BRHitLightingMode=1'); t = 40 }
  'v414'  = @{ a = @('-BRAutoTest', '-BRAutoTestV414', '-BRAutoTestStrict'); t = 40 }
  'v413'  = @{ a = @('-BRAutoTest', '-BRAutoTestV413', '-BRAutoTestStrict'); t = 70 }
  'v412'  = @{ a = @('-BRAutoTest', '-BRAutoTestV412', '-BRAutoTestStrict'); t = 70 }
  'v411'  = @{ a = @('-BRAutoTest', '-BRAutoTestV411', '-BRAutoTestStrict'); t = 50 }
  'v410'  = @{ a = @('-BRAutoTest', '-BRAutoTestV410', '-BRAutoTestStrict', '-BRSeed=4242'); t = 30 }
  'v49'   = @{ a = @('-BRAutoTest', '-BRAutoTestV49', '-BRAutoTestStrict'); t = 30 }
  'v48'   = @{ a = @('-BRAutoTest', '-BRAutoTestV48', '-BRAutoTestStrict'); t = 30 }
  'v47'   = @{ a = @('-BRAutoTest', '-BRAutoTestV47', '-BRAutoTestStrict', '-BRSeed=4242'); t = 30 }
  'pits'  = @{ a = @('-BRAutoTest', '-BRAutoTestPits', '-BRSeed=9'); t = 30 }
  'full'  = @{ a = @('-BRAutoTest', '-BRAutoTestStrict', '-BRSeed=4242'); t = 180 }
}
# -File transmet une liste "a,b,c" en une seule chaine : on la decoupe
$Only = @($Only | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($Only.Count -eq 0) { $Only = @('smoke','v414','v413','v412','v411','v410','v49','v48','v47','pits','full') }
if (-not $Tag) { $Tag = 'base' }
$root = Join-Path (Join-Path $saved 'TestRuns\autotest') $Tag
New-Item -ItemType Directory -Force $root | Out-Null
foreach ($name in $Only) {
  if (-not $runs.Contains($name)) { "$name INCONNU (suites : $($runs.Keys -join ', '))"; continue }
  if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { "$name SKIPPED: un autre Unreal tourne"; continue }
  $r = $runs[$name]
  $dir = Join-Path $root $name
  if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
  New-Item -ItemType Directory -Force $dir | Out-Null
  if (Test-Path "$saved\AutoTest") { Remove-Item -Recurse -Force "$saved\AutoTest" }
  $log = Join-Path $dir 'game.log'
  $argList = @("`"$proj`"") + $common + $r.a + @("-abslog=`"$log`"")
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $p = Start-Process -FilePath $Editor -ArgumentList $argList -PassThru
  $null = $p.Handle
  $done = $p.WaitForExit($r.t * 60 * 1000)
  if (-not $done) { Stop-Process -Id $p.Id -Force; $p.WaitForExit(); $code = 'TIMEOUT' } else { $code = $p.ExitCode }
  $sw.Stop()
  if (Test-Path "$saved\AutoTest") { Copy-Item -Recurse -Force "$saved\AutoTest\*" $dir }
  $res = 'PAS DE RAPPORT'
  if (Test-Path "$dir\Rapport.json") {
    $j = Get-Content "$dir\Rapport.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    $res = "etat=$($j.etat) problemes=$(@($j.problemes).Count) non_verifies=$(@($j.non_verifies).Count)"
  }
  $line = "{0,-6} exit={1} time={2:n1}min {3}" -f $name, $code, $sw.Elapsed.TotalMinutes, $res
  $line
  Add-Content -Path (Join-Path $root 'summary.txt') -Value $line
}
