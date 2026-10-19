<#
v4.16 (perf) : mesures de performance du jeu, N executions x graines x profils, chacune archivee sous une etiquette.

Chaque lancement : UnrealEditor.exe <projet> -game -windowed -ResX -ResY -unattended -nosplash -log -BRAutoTest -BRAutoTestPerf
  -BRPerfTag=<etiquette> -BRPerfRun=<n> [-BRPerfSeed=<graine>] [-BRProfile=<profil>] [-BRPerfScenes=...] [-BRPerfSeconds=<s>]
  [-BRPerfGpu=<k>] [-BRAutoTestStrict] -abslog=<dossier>\game.log
puis copie de Saved\AutoTest (Rapport.txt/json, Perf.csv, PerfPasses.csv, PerfGPU\, PerfFrames\, Mesures.csv) dans
<OutRoot>\<Tag>\run<n>_graine<g>_profil<p>\. A la fin : manifest.json (commit, machine, lignes de commande, resultats),
summary.txt et resume.md (Tools\Tests\perf_summary.py).

Exemples (depuis n'importe quel dossier, PowerShell 5.1) :
  powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Tag base -Runs 3
      3 executions, graines de reference, les 4 profils dans chaque execution (changes en jeu, avant chaque chargement)
  powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Tag base_graines -Runs 2 -Seeds 0,4242,77 -Profiles 0,1,4,2
      2 executions x 3 graines (0 : graines de reference) x 4 profils, un profil par lancement (-BRProfile)
  powershell -ExecutionPolicy Bypass -File Tools\Tests\run_perf.ps1 -Tag fosses -Runs 5 -Scenes fosses,bassins -Profiles Cinematique
Comparer deux etiquettes :
  python Tools\Tests\perf_summary.py --before Saved\PerfRuns\base --after Saved\PerfRuns\opt1 --out comparaison.md
Aucune autre instance d'Unreal ne doit tourner (mesures du processeur graphique) : un lancement est saute sinon.
v4.16 (perf, corrections de relecture) : une etiquette qui contient deja des executions (dossiers run*) est refusee (sortie
2) : perf_summary.py lit tout le dossier de l'etiquette, d'anciennes executions s'y melangeraient sans bruit. -Append
ajoute des executions numerotees a la suite (manifest.json garde les resultats precedents). Chaque dossier d'execution
recoit commit.txt (commit=<sha>, modifie=0|1), que perf_summary.py lit pour signaler une etiquette qui melange plusieurs
commits ou un arbre modifie.
#>
param(
  [string]$Tag = '',
  [int]$Runs = 3,
  [string[]]$Seeds = @('0'),
  [string[]]$Profiles = @('all'),
  [string]$Scenes = '',
  [double]$Seconds = 0,
  [int]$GpuCaptures = -1,
  [int]$ResX = 1920,
  [int]$ResY = 1080,
  [string]$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe',
  [string]$Project = '',
  [string]$OutRoot = '',
  [int]$TimeoutMin = 60,
  [switch]$Strict,
  [switch]$NoSummary,
  [switch]$Append,
  # v4.16 : commandes de la console appliquees apres le profil (-BRPerfCmds, essais ; separees par des points-virgules)
  [string]$PerfCmds = ''
)
$ErrorActionPreference = 'Continue'
$inv = [Globalization.CultureInfo]::InvariantCulture

if (-not $Project) { $Project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Backrooms.uproject')).Path }
$projDir = Split-Path -Parent $Project
$saved = Join-Path $projDir 'Saved'
if (-not $OutRoot) { $OutRoot = Join-Path $saved 'PerfRuns' }
if (-not $Tag) { $Tag = 'perf_' + (Get-Date -Format 'yyyyMMdd_HHmm') }
$Tag = $Tag -replace '[^A-Za-z0-9_.-]', '_'
$tagDir = Join-Path $OutRoot $Tag
# v4.16 (perf, corrections de relecture) : etiquette deja utilisee : refusee, sauf -Append (executions numerotees a la suite)
$firstRun = 1
$existingRuns = @()
if (Test-Path -LiteralPath $tagDir) {
  $existingRuns = @(Get-ChildItem -LiteralPath $tagDir -Directory -ErrorAction SilentlyContinue | ForEach-Object { if ($_.Name -match '^run(\d+)_') { [int]$Matches[1] } })
}
if ($existingRuns.Count -gt 0) {
  if (-not $Append) {
    "L'etiquette $Tag contient deja $($existingRuns.Count) execution(s) ($tagDir) : choisir une autre etiquette, ou -Append pour ajouter des executions a la suite"
    exit 2
  }
  $firstRun = [int](($existingRuns | Measure-Object -Maximum).Maximum) + 1
}
New-Item -ItemType Directory -Force $tagDir | Out-Null
if (-not (Test-Path $Editor)) { "Editeur introuvable : $Editor"; exit 2 }

# Listes (-Seeds 0,4242 arrive en une seule chaine avec -File)
$seedList = @()
foreach ($item in $Seeds) { $seedList += @($item -split '[,; ]+' | Where-Object { $_ -ne '' }) }
if ($seedList.Count -eq 0) { $seedList = @('0') }
$profileList = @()
foreach ($item in $Profiles) {
  foreach ($word in @($item -split '[,; ]+' | Where-Object { $_ -ne '' })) {
    switch -Regex ($word) {
      '^(all|tous)$' { $profileList += 'all' }
      '^(0|performance)$' { $profileList += '0' }
      '^(1|qualite)$' { $profileList += '1' }
      '^(2|cinematique)$' { $profileList += '2' }
      '^(4|rtx|rtxfluide|rtx_fluide)$' { $profileList += '4' }
      default { "Profil inconnu : $word (all, 0 Performance, 1 Qualite, 4 RTXFluide, 2 Cinematique)"; exit 2 }
    }
  }
}
if ($profileList.Count -eq 0) { $profileList = @('all') }

# Contexte : commit, machine
$commit = ''
$dirty = $false
if (Get-Command git -ErrorAction SilentlyContinue) {
  $commit = (& git -C $projDir rev-parse HEAD 2>$null | Select-Object -First 1)
  $dirty = [bool](& git -C $projDir status --porcelain --untracked-files=no 2>$null | Select-Object -First 1)
}
$gpus = @()
try { $gpus = @(Get-CimInstance Win32_VideoController -ErrorAction Stop | ForEach-Object { '{0} (pilote {1})' -f $_.Name, $_.DriverVersion }) } catch { }
$cpuName = ''
try { $cpuName = (Get-CimInstance Win32_Processor -ErrorAction Stop | Select-Object -First 1).Name } catch { }

$summary = Join-Path $tagDir 'summary.txt'
$results = @()
$header = 'etiquette {0} : {1} execution(s) x graines [{2}] x profils [{3}] ; commit {4}{5} ; {6} ; {7}{8}' -f $Tag, $Runs, ($seedList -join ','), ($profileList -join ','), $commit, $(if ($dirty) { ' (modifie)' } else { '' }), ($gpus -join ' / '), $cpuName, $(if ($firstRun -gt 1) { " ; ajout a partir de l'execution $firstRun (-Append)" } else { '' })
$header
Add-Content -Path $summary -Value $header
$dirtyFlag = $(if ($dirty) { '1' } else { '0' })

for ($run = $firstRun; $run -lt ($firstRun + $Runs); $run++) {
  foreach ($seed in $seedList) {
    foreach ($prof in $profileList) {
      $name = 'run{0}_graine{1}_profil{2}' -f $run, $seed, $prof
      if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
        $line = "{0,-34} SAUTE : un autre Unreal tourne" -f $name
        $line
        Add-Content -Path $summary -Value $line
        $results += [ordered]@{ nom = $name; etat = 'saute'; commit = $commit; modifie = $dirty }
        continue
      }
      $dir = Join-Path $tagDir $name
      if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
      New-Item -ItemType Directory -Force $dir | Out-Null
      # v4.16 (perf, corrections de relecture) : identite du code mesure, lue par perf_summary.py
      [IO.File]::WriteAllText((Join-Path $dir 'commit.txt'), ("commit=$commit`r`nmodifie=$dirtyFlag`r`n"), (New-Object System.Text.UTF8Encoding($false)))
      $log = Join-Path $dir 'game.log'
      $gameArgs = @("`"$Project`"", '-game', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-unattended', '-nosplash', '-log', '-BRAutoTest', '-BRAutoTestPerf',
        "-BRPerfTag=$Tag", "-BRPerfRun=$run")
      if ($seed -ne '0') { $gameArgs += "-BRPerfSeed=$seed" }
      if ($prof -ne 'all') { $gameArgs += "-BRProfile=$prof" }
      if ($Scenes) { $gameArgs += "-BRPerfScenes=$Scenes" }
      if ($Seconds -gt 0) { $gameArgs += ('-BRPerfSeconds=' + $Seconds.ToString($inv)) }
      if ($GpuCaptures -ge 0) { $gameArgs += "-BRPerfGpu=$GpuCaptures" }
      if ($Strict) { $gameArgs += '-BRAutoTestStrict' }
      if ($PerfCmds) { $gameArgs += "-BRPerfCmds=`"$PerfCmds`"" }
      $gameArgs += "-abslog=`"$log`""
      $cmdLine = '"' + $Editor + '" ' + ($gameArgs -join ' ')
      Set-Content -Path (Join-Path $dir 'commande.txt') -Value $cmdLine -Encoding ASCII
      if (Test-Path "$saved\AutoTest") { Remove-Item -Recurse -Force "$saved\AutoTest" -ErrorAction SilentlyContinue }
      $sw = [Diagnostics.Stopwatch]::StartNew()
      $proc = Start-Process -FilePath $Editor -ArgumentList $gameArgs -PassThru
      $null = $proc.Handle
      $finished = $proc.WaitForExit($TimeoutMin * 60 * 1000)
      if (-not $finished) { Stop-Process -Id $proc.Id -Force; $proc.WaitForExit(); $code = 'TIMEOUT' } else { $code = $proc.ExitCode }
      $sw.Stop()
      if (Test-Path "$saved\AutoTest") { Copy-Item -Recurse -Force "$saved\AutoTest\*" $dir }
      $state = 'PAS DE RAPPORT'
      $problems = -1
      $skipped = -1
      if (Test-Path "$dir\Rapport.json") {
        $report = Get-Content "$dir\Rapport.json" -Raw -Encoding UTF8 | ConvertFrom-Json
        $state = $report.etat
        $problems = @($report.problemes).Count
        $skipped = @($report.non_verifies).Count
      }
      $rows = 0
      if (Test-Path "$dir\Perf.csv") { $rows = [Math]::Max(0, @(Get-Content "$dir\Perf.csv").Count - 1) }
      $line = "{0,-34} exit={1} duree={2:n1}min etat={3} problemes={4} non_verifies={5} mesures={6}" -f $name, $code, $sw.Elapsed.TotalMinutes, $state, $problems, $skipped, $rows
      $line
      Add-Content -Path $summary -Value $line
      $results += [ordered]@{ nom = $name; execution = $run; graine = $seed; profil = $prof; sortie = "$code"; minutes = [Math]::Round($sw.Elapsed.TotalMinutes, 2); etat = $state; problemes = $problems; non_verifies = $skipped; mesures = $rows; commit = $commit; modifie = $dirty; commande = $cmdLine }
    }
  }
}

# v4.16 (perf, corrections de relecture) : avec -Append, les resultats des lancements precedents restent dans manifest.json
$manifestPath = Join-Path $tagDir 'manifest.json'
$allResults = @()
if ($Append -and (Test-Path $manifestPath)) {
  try {
    $previous = Get-Content $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($previous.resultats) { $allResults += @($previous.resultats) }
  } catch {
    'manifest.json precedent illisible : ses resultats ne sont pas repris (les dossiers run* et leur commit.txt restent)'
  }
}
$allResults += $results
$commitsSeen = @($allResults | ForEach-Object { $_.commit } | Where-Object { $_ } | Sort-Object -Unique)

$manifest = [ordered]@{
  etiquette = $Tag
  date = (Get-Date).ToString('s')
  projet = $Project
  commit = $commit
  modifie = $dirty
  cartes = $gpus
  processeur = $cpuName
  executions = $Runs
  graines = $seedList
  profils = $profileList
  scenes = $Scenes
  secondes = $Seconds
  premiere_execution = $firstRun
  commits = $commitsSeen
  resultats = $allResults
}
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), (New-Object System.Text.UTF8Encoding($false)))

if (-not $NoSummary) {
  $py = Get-Command python -ErrorAction SilentlyContinue
  if ($py) {
    $env:PYTHONUTF8 = '1'
    & $py.Source (Join-Path $PSScriptRoot 'perf_summary.py') $tagDir --out (Join-Path $tagDir 'resume.md')
  } else {
    'python introuvable : resume.md non ecrit (python Tools\Tests\perf_summary.py ' + $tagDir + ')'
  }
}
"Resultats : $tagDir"
