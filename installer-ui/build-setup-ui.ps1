param(
  [Parameter(Mandatory = $true)][string]$Core,
  [Parameter(Mandatory = $true)][string]$Out,
  [string]$DogImage = ""
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$csc = Join-Path ${env:WINDIR} 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path $csc)) { throw "csc.exe not found at $csc" }
if (-not (Test-Path $Core)) { throw "core installer not found: $Core" }
if (-not $DogImage) { $DogImage = Join-Path $root 'resource\doggy\mascot-default.png' }
if (-not (Test-Path $DogImage)) { throw "mascot image not found: $DogImage" }

$source = Join-Path $PSScriptRoot 'SetupUi.cs'
$manifest = Join-Path $PSScriptRoot 'app.manifest'
$arguments = @(
  '/nologo',
  '/target:winexe',
  '/optimize+',
  '/platform:anycpu',
  "/out:$Out",
  "/win32manifest:$manifest",
  "/resource:$Core,CoreInstaller",
  "/resource:$DogImage,DogImage",
  '/r:System.dll',
  '/r:System.Drawing.dll',
  '/r:System.Windows.Forms.dll',
  $source
)
& $csc @arguments
if ($LASTEXITCODE -ne 0) { throw "csc failed with exit code $LASTEXITCODE" }
if (-not (Test-Path $Out)) { throw "compiler produced no output: $Out" }
Write-Host "Setup UI built: $Out ($((Get-Item $Out).Length) bytes)"