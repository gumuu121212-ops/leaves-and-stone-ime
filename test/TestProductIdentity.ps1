$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$generator = Join-Path $repoRoot 'scripts\generate-product-config.ps1'
& $generator

$config = Get-Content -LiteralPath (Join-Path $repoRoot 'config\product.json') -Raw | ConvertFrom-Json
$header = Get-Content -LiteralPath (Join-Path $repoRoot 'include\LeavesStoneProduct.h') -Raw
$nsis = Get-Content -LiteralPath (Join-Path $repoRoot 'output\LeavesStoneProduct.nsh') -Raw

if ($config.display_name -ne '小毛豆输入法') { throw 'Unexpected product display name.' }
if ($config.registry_root -ne 'Software\LeavesAndStone\IME') { throw 'Unexpected product registry root.' }
if ($config.user_data_subdir -ne 'LeavesAndStone\Rime') { throw 'Unexpected user data directory.' }

$productGuids = @(
  $config.text_service_clsid,
  $config.language_profile_guid,
  $config.candidate_ui_guid
)
$upstreamGuids = @(
  'A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A',
  '3D02CAB6-2B8E-4781-BA20-1C9267529467',
  '36C3C795-7159-45AA-AB12-30229A51DBD3'
)

if (($productGuids | Select-Object -Unique).Count -ne 3) { throw 'Product GUIDs must be unique.' }
foreach ($guid in $productGuids) {
  if ($guid -in $upstreamGuids) { throw "Product GUID reuses upstream identity: $guid" }
  if (-not [guid]::TryParse($guid, [ref]([guid]::Empty))) { throw "Invalid GUID: $guid" }
}

if ($header -notmatch 'kDisplayName\[\].*小毛豆输入法') { throw 'Generated C++ header lost the display name.' }
if ($header -notmatch 'kRegistryRoot\[\].*Software\\\\LeavesAndStone\\\\IME') { throw 'Generated C++ header lost the registry root.' }
if (-not $header.Contains('inline constexpr wchar_t kRegistryRootWow64[] = L"Software\\WOW6432Node\\LeavesAndStone\\IME";')) {
  throw 'Generated C++ header has an incorrectly escaped WOW64 registry root.'
}
if (-not $header.Contains('inline constexpr wchar_t kRegistryUpdates[] = L"Software\\LeavesAndStone\\IME\\Updates";')) {
  throw 'Generated C++ header has an incorrectly escaped update registry path.'
}
if ($nsis -notmatch 'LEAVES_STONE_DISPLAY_NAME "小毛豆输入法"') { throw 'Generated NSIS constants lost the display name.' }
if ($nsis -notmatch 'LEAVES_STONE_REGISTRY_ROOT "Software\\LeavesAndStone\\IME"') { throw 'Generated NSIS constants lost the registry root.' }

$globals = Get-Content -LiteralPath (Join-Path $repoRoot 'WeaselTSF\Globals.cpp') -Raw
$candidateList = Get-Content -LiteralPath (Join-Path $repoRoot 'WeaselTSF\CandidateList.cpp') -Raw
$utility = Get-Content -LiteralPath (Join-Path $repoRoot 'RimeWithWeasel\WeaselUtility.cpp') -Raw
$installer = Get-Content -LiteralPath (Join-Path $repoRoot 'output\install.nsi') -Raw
if ($globals -notmatch 'kTextServiceClsid' -or $globals -notmatch 'kLanguageProfileGuid') { throw 'TSF identity is not wired to generated constants.' }
if ($candidateList -notmatch 'kCandidateUiGuid') { throw 'Candidate UI identity is not wired to generated constants.' }
if ($utility -notmatch 'kRegistryRoot' -or $utility -notmatch 'kUserDataSubdir') { throw 'Runtime data paths are not wired to generated constants.' }
if ($installer -match 'Software\\Rime\\Weasel' -or $installer -match 'Uninstall\\Weasel') { throw 'Installer still activates an upstream Weasel registry identity.' }
if ($installer -notmatch 'leaves-and-stone-\$\{PRODUCT_VERSION\}-installer\.exe') { throw 'Installer filename is not product-specific.' }

$resourceFiles = @(
  'WeaselSetup\WeaselSetup.rc',
  'WeaselDeployer\WeaselDeployer.rc',
  'WeaselServer\WeaselServer.rc'
)
foreach ($resourceFile in $resourceFiles) {
  $resourceText = [System.IO.File]::ReadAllText((Join-Path $repoRoot $resourceFile), [System.Text.Encoding]::Unicode)
  if ($resourceText -match 'VALUE "ProductName", "Weasel"') { throw "$resourceFile still exposes the upstream product name." }
  if ($resourceText -notmatch 'VALUE "ProductName", "Leaves & Stone"') { throw "$resourceFile does not expose the Leaves & Stone product name." }
}

$runtimeIdentityFiles = @(
  'include\WeaselConstants.h',
  'include\WeaselUtility.h',
  'WeaselDeployer\SwitcherSettingsDialog.cpp',
  'WeaselServer\WeaselServerApp.cpp',
  'WeaselServer\WeaselServerApp.h',
  'WeaselSetup\imesetup.cpp',
  'WeaselSetup\WeaselSetup.cpp',
  'WeaselTSF\LanguageBar.cpp',
  'WeaselTSF\WeaselTSF.cpp',
  'get-rime.ps1'
)
foreach ($runtimeFile in $runtimeIdentityFiles) {
  $runtimeText = Get-Content -LiteralPath (Join-Path $repoRoot $runtimeFile) -Raw
  if ($runtimeText -match 'Software\\+Rime\\+[Ww]easel') {
    throw "$runtimeFile still activates the upstream Weasel registry identity."
  }
}

$tsfResource = [System.IO.File]::ReadAllText((Join-Path $repoRoot 'WeaselTSF\WeaselTSF.rc'), [System.Text.Encoding]::Unicode)
if ($tsfResource -match 'VALUE "ProductName", "Weasel"') { throw 'WeaselTSF.rc still exposes the upstream product name.' }

Write-Host 'TestProductIdentity: PASS'
