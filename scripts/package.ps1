[CmdletBinding()]
param(
  [ValidateSet('Debug', 'Release')]
  [string]$Configuration = 'Release',

  [ValidateSet('Win32', 'x64', 'All')]
  [string]$Platform = 'All',

  [string]$OutDirectory = 'output\artifacts',

  [switch]$SkipBuild,
  [switch]$Sign,
  [string]$Thumbprint,
  [string]$PfxPath,
  [securestring]$PfxPassword
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$archivesDirectory = Join-Path $repoRoot 'output\archives'
$artifactsDirectory = Join-Path $repoRoot $OutDirectory

# A release folder that ships a private key has leaked it, so refuse to pack.
# Vendored submodules are skipped: their upstream test fixtures are not ours
# and must not block every package run.
$secretPatterns = @('*.pfx', '*.p12', '*.key', '*.pem', '*.snk')
$leaked = foreach ($pattern in $secretPatterns) {
  Get-ChildItem -Path $repoRoot -Filter $pattern -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch '\\\.git\\|\\librime\\|\\plum\\' }
}
$leaked = @($leaked)
if ($leaked.Count -gt 0) {
  $list = ($leaked | ForEach-Object { $_.FullName }) -join '; '
  throw "Private key material is present in the repository: $list. Remove it before packaging."
}

if (-not $SkipBuild) {
  & (Join-Path $PSScriptRoot 'build-local.ps1') -Configuration $Configuration -Platform $Platform
  if ($LASTEXITCODE -ne 0) {
    throw "build-local.ps1 failed with exit code $LASTEXITCODE."
  }
}

$installer = Get-ChildItem -LiteralPath $archivesDirectory -Filter '*-installer.exe' -File -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
if (-not $installer) {
  throw "No installer was found in $archivesDirectory. Build with -Configuration Release (the installer step requires NSIS)."
}

if ($Sign) {
  $signArguments = @{}
  if ($Thumbprint) { $signArguments['Thumbprint'] = $Thumbprint }
  if ($PfxPath) { $signArguments['PfxPath'] = $PfxPath }
  if ($PfxPassword) { $signArguments['PfxPassword'] = $PfxPassword }
  & (Join-Path $PSScriptRoot 'sign-build.ps1') @signArguments
  if ($LASTEXITCODE -ne 0) {
    throw "sign-build.ps1 failed with exit code $LASTEXITCODE."
  }
}

New-Item -ItemType Directory -Force -Path $artifactsDirectory | Out-Null
$installerCopy = Join-Path $artifactsDirectory $installer.Name
Copy-Item -LiteralPath $installer.FullName -Destination $installerCopy -Force

$payload = @(
  $installerCopy
  Get-ChildItem -Path (Join-Path $repoRoot 'output\*') -Include '*.exe', '*.dll' -File -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty FullName
  Get-ChildItem -Path (Join-Path $repoRoot 'output\Win32\*') -Include '*.exe', '*.dll' -File -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty FullName
) | Where-Object { $_ } | Select-Object -Unique

$hashes = foreach ($file in $payload) {
  [pscustomobject]@{
    file = $file.Replace($repoRoot + '\', '')
    sha256 = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    bytes = (Get-Item -LiteralPath $file).Length
  }
}
$hashes = @($hashes)

$sumsPath = Join-Path $artifactsDirectory 'SHA256SUMS.txt'
$hashes | ForEach-Object { '{0}  {1}' -f $_.sha256, $_.file } | Set-Content -LiteralPath $sumsPath -Encoding ascii

$commit = (& git -C $repoRoot rev-parse HEAD) 2>$null
$branch = (& git -C $repoRoot rev-parse --abbrev-ref HEAD) 2>$null
$manifest = [ordered]@{
  product = 'Leaves & Stone'
  installer = $installer.Name
  installer_sha256 = (Get-FileHash -LiteralPath $installerCopy -Algorithm SHA256).Hash
  configuration = $Configuration
  platform = $Platform
  commit = if ($commit) { $commit.Trim() } else { 'unknown' }
  branch = if ($branch) { $branch.Trim() } else { 'unknown' }
  signed = [bool]$Sign
  file_count = $hashes.Count
  files = $hashes
}
$manifestPath = Join-Path $artifactsDirectory 'manifest.json'
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8

Write-Output "installer=$installerCopy"
Write-Output "sums=$sumsPath"
Write-Output "manifest=$manifestPath"
Write-Output "signed=$([bool]$Sign)"
if (-not $Sign) {
  Write-Output 'This package is unsigned. Distribute it only as a test build.'
}
