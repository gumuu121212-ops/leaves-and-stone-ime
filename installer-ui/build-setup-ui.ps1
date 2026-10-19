param(
  [Parameter(Mandatory = $true)][string]$Core,
  [Parameter(Mandatory = $true)][string]$Out
)
$ErrorActionPreference = 'Stop'
$csc = Join-Path ${env:WINDIR} 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path $csc)) { throw "csc.exe not found at $csc" }
if (-not (Test-Path $Core)) { throw "core installer not found: $Core" }

$root = $PSScriptRoot
$arguments = @(
  '/nologo',
  '/target:winexe',
  '/optimize+',
  '/platform:anycpu',
  "/out:$Out",
  "/win32manifest:$root\app.manifest",
  "/resource:$Core,CoreInstaller",
  "/resource:$root\layout.json,LayoutJson",
  "/resource:$root\assets\mascot.png,img-mascot",
  "/resource:$root\assets\paw.png,img-paw",
  "/resource:$root\assets\cushion.png,img-cushion",
  "/resource:$root\assets\titlebar-dog.png,img-titlebar-dog",
  '/r:System.dll',
  '/r:System.Drawing.dll',
  '/r:System.Windows.Forms.dll',
  '/r:System.Web.Extensions.dll',
  "$root\SetupUi.cs"
)
& $csc @arguments
if ($LASTEXITCODE -ne 0) { throw "csc failed with exit code $LASTEXITCODE" }
if (-not (Test-Path $Out)) { throw "compiler produced no output: $Out" }
Write-Host "Setup UI built: $Out ($((Get-Item $Out).Length) bytes)"