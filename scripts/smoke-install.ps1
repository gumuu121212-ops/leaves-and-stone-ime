[CmdletBinding()]
param(
  [string]$InstallerPath,
  [int]$TimeoutSeconds = 300,
  [switch]$KeepInstalled
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
  [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
  throw 'The installer writes to HKLM and Program Files. Run this smoke test from an elevated shell.'
}

if (-not $InstallerPath) {
  $InstallerPath = Get-ChildItem -LiteralPath (Join-Path $repoRoot 'output\archives') -Filter '*-installer.exe' -File -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1 -ExpandProperty FullName
}
if (-not $InstallerPath -or -not (Test-Path -LiteralPath $InstallerPath)) {
  throw 'No installer was found. Build it first, or pass -InstallerPath.'
}
$InstallerPath = (Resolve-Path -LiteralPath $InstallerPath).Path

$registryRoot = 'HKLM:\Software\LeavesAndStone\IME'
$uninstallKey = 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\LeavesAndStone'
$userDataRoot = Join-Path $env:APPDATA 'LeavesAndStone\Rime'

function Invoke-Installer {
  param([string]$Path, [string]$Arguments)
  $process = Start-Process -FilePath $Path -ArgumentList $Arguments -PassThru -Wait
  if ($process.ExitCode -ne 0) {
    throw "$Path $Arguments exited with $($process.ExitCode)."
  }
}

function Get-UserDataSnapshot {
  if (-not (Test-Path -LiteralPath $userDataRoot)) { return @() }
  return @(
    Get-ChildItem -LiteralPath $userDataRoot -Recurse -File -ErrorAction SilentlyContinue |
      ForEach-Object { '{0}:{1}' -f $_.FullName, $_.Length }
  )
}

$failures = @()
function Assert-True {
  param([bool]$Condition, [string]$Message)
  if ($Condition) { Write-Output "  ok   $Message" }
  else { Write-Output "  FAIL $Message"; $script:failures += $Message }
}

$dataBefore = Get-UserDataSnapshot
Write-Output "installer=$InstallerPath"
Write-Output "user_data_files_before=$($dataBefore.Count)"

Write-Output '== install =='
Invoke-Installer -Path $InstallerPath -Arguments '/S'
Assert-True (Test-Path -LiteralPath $registryRoot) "registry root $registryRoot exists"
Assert-True (Test-Path -LiteralPath $uninstallKey) "uninstall entry $uninstallKey exists"

$installDir = $null
if (Test-Path -LiteralPath $registryRoot) {
  $installDir = (Get-ItemProperty -LiteralPath $registryRoot -Name InstallDir -ErrorAction SilentlyContinue).InstallDir
}
Assert-True ([bool]$installDir) 'InstallDir is recorded'
if ($installDir) {
  Assert-True (Test-Path -LiteralPath $installDir) "install directory $installDir exists"
  $expected = @('uninstall.exe', 'WeaselServer.exe', 'WeaselSetup.exe', 'WeaselDeployer.exe')
  foreach ($name in $expected) {
    Assert-True (Test-Path -LiteralPath (Join-Path $installDir $name)) "$name is installed"
  }
  $ime = Get-ChildItem -LiteralPath $installDir -Filter '*.ime' -File -ErrorAction SilentlyContinue
  Assert-True (@($ime).Count -gt 0) 'at least one .ime text service is installed'
}

Write-Output '== repair (install over the existing copy) =='
Invoke-Installer -Path $InstallerPath -Arguments '/S'
Assert-True (Test-Path -LiteralPath $registryRoot) 'registry root survives repair'

$dataAfterRepair = Get-UserDataSnapshot
Assert-True ($dataAfterRepair.Count -ge $dataBefore.Count) 'user data is not truncated by repair'

if ($KeepInstalled) {
  Write-Output 'Keeping the installation in place (-KeepInstalled).'
}
else {
  Write-Output '== uninstall =='
  $uninstaller = if ($installDir) { Join-Path $installDir 'uninstall.exe' } else { $null }
  if ($uninstaller -and (Test-Path -LiteralPath $uninstaller)) {
    Invoke-Installer -Path $uninstaller -Arguments '/S'
  }
  else {
    $script:failures += 'uninstall.exe was not found'
    Write-Output '  FAIL uninstall.exe was not found'
  }
  Start-Sleep -Seconds 2
  Assert-True (-not (Test-Path -LiteralPath $registryRoot)) "registry root $registryRoot was removed"
  Assert-True (-not (Test-Path -LiteralPath $uninstallKey)) "uninstall entry $uninstallKey was removed"

  $dataAfterUninstall = Get-UserDataSnapshot
  Assert-True ($dataAfterUninstall.Count -ge $dataBefore.Count) 'user dictionary and settings survive uninstall'
}

if ($failures.Count -gt 0) {
  throw ("Smoke install failed: " + ($failures -join '; '))
}

Write-Output 'Smoke install passed.'
