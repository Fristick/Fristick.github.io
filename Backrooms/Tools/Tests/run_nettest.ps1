# v4.16 : test reseau sur ce PC : l'hote (partie ouverte "?listen") puis 1 a 3 clients 127.0.0.1, un client lance en cours de partie
# ou une reconnexion, latence, gigue et pertes simulees par le moteur sur chaque machine. Fonctionne depuis n'importe quelle copie
# du depot (projet trouve depuis l'emplacement du script ; moteur : -Editor, $env:UE_EDITOR, le registre d'Epic, puis
# C:\Program Files\Epic Games\UE_5.8).
#
#   powershell -ExecutionPolicy Bypass -File Tools\Tests\run_nettest.ps1 -Clients 3 -NullRhiClients -Tag net4
#   ... -Clients 1 -LateJoin porte -Tag tardif_porte          client lance quand l'hote a ouvert la porte du Level 0
#   ... -Clients 1 -LateJoin lidar | passage                  client lance pendant la mission LiDAR du Level 6
#   ... -Clients 1 -LateJoin reconnexion                      le client se reconnecte pendant un chargement
#   ... -Clients 1 -Lag 150 -Jitter 40 -Loss 3 -Tag lag      reseau simule (ms, ms, %) sur l'hote et chaque client
#   ... -Classic                                              plan reseau ordinaire (-BRNetTest, 2 joueurs, v4.15)
# Options : -NullRhiClients (tous les clients sauf le premier sans rendu, -nullrhi), -NullRhiCount <n> (les n derniers clients),
# -Seed <n> (graine du Level 0 de l'hote, 9), -ResX/-ResY (fenetres), -OutRoot (dossier des resultats ; defaut
# Saved\TestRuns\autotest), -TimeoutMin (90), -Editor <UnrealEditor.exe>.
# Resultats : <OutRoot>\<Tag>\<machine>\ (Rapport.txt, Rapport.json, V416.txt, Mesures.csv, captures, game.log) et summary.txt ;
# machines : Hote, Client1..Client3, Tardif (plan ordinaire : Hote, Client). Code de sortie : 0 tout reussi, 1 un probleme,
# 2 seulement des verifications non faites, 3 un rapport manquant.
param(
	[ValidateRange(0, 3)][int]$Clients = 1,
	[switch]$NullRhiClients,
	[int]$NullRhiCount = -1,
	[ValidateSet('none', 'porte', 'lidar', 'passage', 'reconnexion')][string]$LateJoin = 'none',
	[ValidateRange(0, 2000)][int]$Lag = 0,
	[ValidateRange(0, 1000)][int]$Jitter = 0,
	[ValidateRange(0, 100)][int]$Loss = 0,
	[string]$Tag = 'net416',
	[string]$OutRoot = '',
	[string]$Editor = '',
	[int]$Seed = 9,
	[int]$ResX = 1280,
	[int]$ResY = 720,
	[int]$TimeoutMin = 90,
	[switch]$Classic
)
$ErrorActionPreference = 'Continue'

# ---------------------------------------------------------------------------------------------------- projet et moteur
$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Project = Join-Path $ProjectDir 'Backrooms.uproject'
if (-not (Test-Path $Project)) { Write-Error "Projet introuvable : $Project"; exit 3 }
$SavedDir = Join-Path $ProjectDir 'Saved'
if (-not $Editor) { $Editor = $env:UE_EDITOR }
if (-not $Editor) {
	$Assoc = ''
	try { $Assoc = (Get-Content $Project -Raw | ConvertFrom-Json).EngineAssociation } catch { $Assoc = '' }
	if ($Assoc) {
		foreach ($Key in @("HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Assoc", "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$Assoc")) {
			$Dir = (Get-ItemProperty -Path $Key -Name InstalledDirectory -ErrorAction SilentlyContinue).InstalledDirectory
			if ($Dir -and (Test-Path (Join-Path $Dir 'Engine\Binaries\Win64\UnrealEditor.exe'))) { $Editor = Join-Path $Dir 'Engine\Binaries\Win64\UnrealEditor.exe'; break }
		}
	}
}
if (-not $Editor) { $Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' }
if (-not (Test-Path $Editor)) { Write-Error "UnrealEditor.exe introuvable : $Editor (-Editor ou UE_EDITOR)"; exit 3 }
if (-not $OutRoot) { $OutRoot = Join-Path $SavedDir 'TestRuns\autotest' }
$OutDir = Join-Path $OutRoot $Tag
if (Test-Path $OutDir) { Remove-Item -Recurse -Force $OutDir }
New-Item -ItemType Directory -Force $OutDir | Out-Null
if ($Classic) { $Clients = 1; $LateJoin = 'none' }
if ($LateJoin -eq 'reconnexion' -and $Clients -lt 1) { $Clients = 1 }
# Partie limitee a 4 joueurs (DefaultGame.ini, MaxPlayers) : le client tardif est le 4e au plus
if (($LateJoin -eq 'porte' -or $LateJoin -eq 'lidar' -or $LateJoin -eq 'passage') -and $Clients -gt 2) {
	Write-Warning "Arrivee tardive : $Clients clients demandes, 2 lances (l'hote, 2 clients et le client tardif font 4 joueurs)"
	$Clients = 2
}
if ($NullRhiClients -and $NullRhiCount -lt 0) { $NullRhiCount = [Math]::Max(0, $Clients - 1) }
if ($NullRhiCount -lt 0) { $NullRhiCount = 0 }

# Dossiers des rapports des machines (Saved\NetTest_<nom>) : ceux d'un essai precedent ne doivent pas etre compares
Get-ChildItem -Path $SavedDir -Directory -Filter 'NetTest_*' -ErrorAction SilentlyContinue | ForEach-Object { Remove-Item -Recurse -Force $_.FullName }

$Common = @('-game', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-BRNetTest', '-unattended', '-nosplash')
if (-not $Classic) {
	$Common += '-BRAutoTestV416Net'
	if ($LateJoin -ne 'none') { $Common += "-BRNetV416Late=$LateJoin" }
}
if ($Lag -gt 0) { $Common += "-PktLag=$Lag" }
if ($Jitter -gt 0) { $Common += "-PktLagVariance=$Jitter" }
if ($Loss -gt 0) { $Common += "-PktLoss=$Loss" }

$Procs = [ordered]@{}
$Sw = [Diagnostics.Stopwatch]::StartNew()

function Start-Machine([string]$Name, [string[]]$Extra, [int]$WinX, [int]$WinY) {
	$Dir = Join-Path $OutDir $Name
	New-Item -ItemType Directory -Force $Dir | Out-Null
	$Log = Join-Path $Dir 'game.log'
	$ArgList = @("`"$Project`"") + $Extra + $Common + @("-WinX=$WinX", "-WinY=$WinY", "-abslog=`"$Log`"")
	if (-not $Classic -or $Name -ne 'Client') { $ArgList += "-BRNetTestName=$Name" }
	Write-Host ("{0,6:n0} s  lancement {1} : {2}" -f $Sw.Elapsed.TotalSeconds, $Name, ($ArgList -join ' '))
	$P = Start-Process -FilePath $Editor -ArgumentList $ArgList -PassThru
	$null = $P.Handle
	$script:Procs[$Name] = $P
	return $Log
}

function Wait-LogLine([string]$Log, [string]$Pattern, [int]$Seconds, [System.Diagnostics.Process]$Proc) {
	$W = [Diagnostics.Stopwatch]::StartNew()
	while ($W.Elapsed.TotalSeconds -lt $Seconds) {
		if ((Test-Path $Log) -and (Select-String -Path $Log -Pattern $Pattern -Quiet)) { return $true }
		if ($Proc -and $Proc.HasExited) { return $false }
		Start-Sleep -Seconds 2
	}
	return $false
}

# ---------------------------------------------------------------------------------------------------- hote, puis clients
$HostLog = Start-Machine 'Hote' @('"/Game/Backrooms/Maps/L_Backrooms?listen?BRLevel=0"', "-BRSeed=$Seed", "-BRNetV416Clients=$Clients") 0 30
$Ready = Wait-LogLine $HostLog 'Test multijoueur \(hote\)' 240 $Procs['Hote']
"{0,6:n0} s  hote pret : {1}" -f $Sw.Elapsed.TotalSeconds, $Ready
$Width = [Math]::Max(320, [int]($ResX / 2))
for ($K = 1; $K -le $Clients; $K++) {
	$Name = if ($Classic) { 'Client' } else { "Client$K" }
	$Extra = @('127.0.0.1')
	if ($K -gt ($Clients - $NullRhiCount)) { $Extra += '-nullrhi' }
	$ClientLog = Start-Machine $Name $Extra ($Width * $K) (30 + 40 * $K)
	# Un client a la fois (memoire, compilation des shaders) : le suivant quand celui-ci a rejoint la partie
	$Joined = Wait-LogLine $ClientLog 'Test multijoueur \(client\)' 300 $Procs[$Name]
	"{0,6:n0} s  {1} dans la partie : {2}" -f $Sw.Elapsed.TotalSeconds, $Name, $Joined
}
if ($LateJoin -eq 'porte' -or $LateJoin -eq 'lidar' -or $LateJoin -eq 'passage') {
	# Client tardif : lance quand l'hote l'annonce (porte ouverte, mission avancee)
	$Wanted = Wait-LogLine $HostLog 'V416 CLIENT TARDIF ATTENDU' (40 * 60) $Procs['Hote']
	"{0,6:n0} s  hote pret pour le client tardif : {1}" -f $Sw.Elapsed.TotalSeconds, $Wanted
	if ($Wanted) {
		$null = Start-Machine 'Tardif' @('127.0.0.1', '-BRNetV416Tardif') ($Width * ($Clients + 1)) 80
	}
}

# ---------------------------------------------------------------------------------------------------- attente et rapports
$Deadline = [Diagnostics.Stopwatch]::StartNew()
foreach ($Name in @($Procs.Keys)) {
	$P = $Procs[$Name]
	$Left = [Math]::Max(1000, [int](($TimeoutMin * 60 - $Deadline.Elapsed.TotalSeconds) * 1000))
	if (-not $P.WaitForExit($Left)) {
		"{0,6:n0} s  {1} : TEMPS DEPASSE, arrete" -f $Sw.Elapsed.TotalSeconds, $Name
		Stop-Process -Id $P.Id -Force -ErrorAction SilentlyContinue
	}
}
"duree totale {0:n1} min" -f $Sw.Elapsed.TotalMinutes
$Summary = @()
$Worst = 0
foreach ($Name in @($Procs.Keys)) {
	$Dir = Join-Path $OutDir $Name
	$Src = Join-Path $SavedDir "NetTest_$Name"
	if ($Classic -and $Name -eq 'Hote') { $Src = Join-Path $SavedDir 'NetTest_Hote' }
	if (Test-Path $Src) { Copy-Item -Recurse -Force (Join-Path $Src '*') $Dir }
	$Code = if ($Procs[$Name].HasExited) { $Procs[$Name].ExitCode } else { 'ARRETE' }
	$Json = Join-Path $Dir 'Rapport.json'
	if (Test-Path $Json) {
		$J = Get-Content $Json -Raw -Encoding UTF8 | ConvertFrom-Json
		$Line = "{0,-8} sortie={1} etat={2} problemes={3} non_verifies={4}" -f $Name, $Code, $J.etat, @($J.problemes).Count, @($J.non_verifies).Count
		$Summary += $Line
		$J.problemes | ForEach-Object { $Summary += "   PROBLEME: $_" }
		$J.non_verifies | ForEach-Object { $Summary += "   NON VERIFIE: $_" }
		if (@($J.problemes).Count -gt 0) { $Worst = [Math]::Max($Worst, 1) } elseif (@($J.non_verifies).Count -gt 0 -and $Worst -lt 1) { $Worst = 2 }
	} else {
		$Summary += ("{0,-8} sortie={1} PAS DE RAPPORT" -f $Name, $Code)
		$Worst = 3
	}
}
$Head = "run_nettest {0} : clients {1} (sans rendu {2}), arrivee tardive {3}, latence {4} ms, gigue {5} ms, pertes {6} %, plan {7}" -f $Tag, $Clients, $NullRhiCount,
	$LateJoin, $Lag, $Jitter, $Loss, $(if ($Classic) { 'ordinaire (-BRNetTest)' } else { 'v4.16 (-BRAutoTestV416Net)' })
$Summary = @($Head) + $Summary
$Summary | Set-Content -Path (Join-Path $OutDir 'summary.txt') -Encoding UTF8
$Summary
exit $Worst
