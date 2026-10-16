$ErrorActionPreference = 'Stop'

$scriptPath = Join-Path $PSScriptRoot '..\scripts\build-local.ps1'
$debugCommand = & $scriptPath -Configuration Debug -Platform x64 -DryRun
$releaseCommand = & $scriptPath -Configuration Release -Platform All -DryRun

if ($debugCommand -notmatch 'build\.bat debug weasel') {
  throw "Unexpected Debug command: $debugCommand"
}

if ($releaseCommand -notmatch 'build\.bat release weasel installer') {
  throw "Unexpected Release command: $releaseCommand"
}

Write-Host 'TestBuildLocal: PASS'

