<#
v4.17 : rejoue l'import des ressources (Content/Python/backrooms_setup.py) dans l'editeur, sans surveillance.

  powershell -ExecutionPolicy Bypass -File Tools\Build\run_editor_setup.ps1 [-Force] [-Kit] [-TimeoutMin 40]
-Force : tout reimporter (backrooms_setup.run(force=True)) ; -Kit : kit du Level 0 reimporte (FBX regeneres).
Resultat : Saved\BackroomsSetup_Result.txt (RESULT=OK|PROBLEMS|EXCEPTION) et journal Saved\Logs\editor_setup.log ;
code de sortie 0 si RESULT=OK, 1 sinon. Aucune autre instance d'Unreal ne doit tourner.
#>
param(
  [switch]$Force,
  [switch]$Kit,
  [int]$TimeoutMin = 40,
  [string]$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
)
$ErrorActionPreference = 'Continue'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\Backrooms.uproject')).Path
$projDir = Split-Path -Parent $project
$saved = Join-Path $projDir 'Saved'
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { 'un autre Unreal tourne : rien lance'; exit 1 }
$result = Join-Path $saved 'BackroomsSetup_Result.txt'
if (Test-Path $result) { Remove-Item -Force $result }
$log = Join-Path $saved 'Logs\editor_setup.log'
$env:BR_SETUP_FORCE = if ($Force) { '1' } else { '0' }
$env:BR_SETUP_KIT = if ($Kit) { '1' } else { '0' }
$env:BR_SETUP_OUT = $result
$script = Join-Path $PSScriptRoot 'editor_setup.py'
$argList = @("`"$project`"", "-ExecutePythonScript=`"$script`"", '-unattended', '-nosplash', '-log', "-abslog=`"$log`"")
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $Editor -ArgumentList $argList -PassThru
$null = $p.Handle
if (-not $p.WaitForExit($TimeoutMin * 60 * 1000)) { Stop-Process -Id $p.Id -Force; 'TIMEOUT'; exit 1 }
$sw.Stop()
"duree {0:n1} min, code {1}" -f $sw.Elapsed.TotalMinutes, $p.ExitCode
if (Test-Path $result) { Get-Content $result } else { 'PAS DE RESULTAT'; exit 1 }
if ((Get-Content $result -First 1) -eq 'RESULT=OK') { exit 0 } else { exit 1 }
