[CmdletBinding()]
param(
  [ValidateSet('Debug', 'Release')]
  [string]$Configuration = 'Debug',

  [ValidateSet('Win32', 'x64', 'All')]
  [string]$Platform = 'x64',

  [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $repoRoot 'build.bat'

$arguments = if ($Configuration -eq 'Debug') {
  @('debug', 'weasel')
} else {
  @('release', 'weasel', 'installer')
}

# The upstream Weasel wrapper builds Win32 and x64 together. Platform remains
# explicit in this public interface so a future split build can preserve callers.
$displayCommand = 'build.bat ' + ($arguments -join ' ')
if ($DryRun) {
  Write-Output $displayCommand
  return
}

$verifyScript = Join-Path $PSScriptRoot 'verify-toolchain.ps1'
. $verifyScript -NoExit
$toolchain = Get-LeavesStoneToolchainStatus
if (-not $toolchain.Ready) {
  throw 'Toolchain verification failed.'
}

Push-Location $repoRoot
try {
  & $buildScript @arguments
  if ($LASTEXITCODE -ne 0) {
    throw "$displayCommand failed with exit code $LASTEXITCODE."
  }
} finally {
  Pop-Location
}

