$ErrorActionPreference = 'Stop'

$scriptPath = Join-Path $PSScriptRoot '..\scripts\verify-toolchain.ps1'
. $scriptPath -NoExit

$status = Get-LeavesStoneToolchainStatus -BoostRoot 'Z:\definitely-missing-boost'

if ($status.Boost.Found) {
  throw 'An invalid Boost root must not be reported as found.'
}

if ($status.MissingRequired -notcontains 'BOOST_ROOT') {
  throw 'MissingRequired must name BOOST_ROOT.'
}

if (-not $status.Git.Found) {
  throw 'Git should be detected on the development host.'
}

Write-Host 'TestToolchain: PASS'

