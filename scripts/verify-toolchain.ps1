[CmdletBinding()]
param(
  [string]$BoostRoot = $env:BOOST_ROOT,
  [switch]$NoExit
)

$ErrorActionPreference = 'Stop'

function Get-CommandStatus {
  param(
    [Parameter(Mandatory)] [string]$Name,
    [string[]]$FallbackPaths = @()
  )

  $command = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
  $path = if ($command) { $command.Source } else { $FallbackPaths | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1 }
  [pscustomobject]@{
    Found = [bool]$path
    Path = $path
  }
}

function Get-LeavesStoneToolchainStatus {
  [CmdletBinding()]
  param([string]$BoostRoot = $env:BOOST_ROOT)

  $programFilesX86 = [Environment]::GetFolderPath('ProgramFilesX86')
  $windowsKits = Join-Path $programFilesX86 'Windows Kits\10\bin'
  $signTool = Get-ChildItem -LiteralPath $windowsKits -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1

  $msbuildFallback = Get-ChildItem -LiteralPath (Join-Path $programFilesX86 'Microsoft Visual Studio') -Filter MSBuild.exe -Recurse -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -ExpandProperty FullName -First 1

  $nsisFallback = @(
    (Join-Path $programFilesX86 'NSIS\makensis.exe'),
    (Join-Path $programFilesX86 'NSIS\Bin\makensis.exe')
  )

  $status = [ordered]@{
    MSBuild = Get-CommandStatus -Name 'msbuild.exe' -FallbackPaths @($msbuildFallback)
    WindowsSdk = [pscustomobject]@{ Found = [bool]$signTool; Path = $signTool.FullName }
    CMake = Get-CommandStatus -Name 'cmake.exe'
    Boost = [pscustomobject]@{ Found = [bool]($BoostRoot -and (Test-Path -LiteralPath ([System.IO.Path]::Combine($BoostRoot, 'boost')))); Path = $BoostRoot }
    NSIS = Get-CommandStatus -Name 'makensis.exe' -FallbackPaths $nsisFallback
    Git = Get-CommandStatus -Name 'git.exe'
    ClangFormat = Get-CommandStatus -Name 'clang-format.exe'
  }

  $requiredMap = [ordered]@{
    MSBuild = 'MSBuild'
    WindowsSdk = 'Windows SDK / SignTool'
    CMake = 'CMake'
    Boost = 'BOOST_ROOT'
    NSIS = 'NSIS'
    Git = 'Git'
    ClangFormat = 'clang-format'
  }
  $missing = foreach ($key in $requiredMap.Keys) {
    if (-not $status[$key].Found) { $requiredMap[$key] }
  }

  [pscustomobject]@{
    MSBuild = $status.MSBuild
    WindowsSdk = $status.WindowsSdk
    CMake = $status.CMake
    Boost = $status.Boost
    NSIS = $status.NSIS
    Git = $status.Git
    ClangFormat = $status.ClangFormat
    MissingRequired = @($missing)
    Ready = @($missing).Count -eq 0
  }
}

if (-not $NoExit) {
  $result = Get-LeavesStoneToolchainStatus -BoostRoot $BoostRoot
  $result | ConvertTo-Json -Depth 4
  if (-not $result.Ready) {
    Write-Error ('Missing required build dependencies: ' + ($result.MissingRequired -join ', '))
    exit 1
  }
}

