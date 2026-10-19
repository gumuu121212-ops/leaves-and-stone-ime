[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

$required = @(
  'LeavesStoneSettings\LeavesStoneSettings.vcxproj',
  'LeavesStoneSettings\App.cpp',
  'LeavesStoneSettings\SettingsDialog.h',
  'LeavesStoneSettings\SettingsDialog.cpp',
  'LeavesStoneSettings\SettingsStore.h',
  'LeavesStoneSettings\SettingsStore.cpp',
  'test\TestLeavesStoneUI\SettingsStoreTests.cpp',
  'test\TestLeavesStoneUI\InputStateTests.cpp',
  'WeaselTSF\LeavesStoneInputState.h',
  'WeaselTSF\LeavesStoneInputState.cpp'
)
foreach ($relative in $required) {
  if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $relative))) {
    throw "Missing Task 6 file: $relative"
  }
}

$store = Get-Content -LiteralPath (Join-Path $repoRoot 'LeavesStoneSettings\SettingsStore.cpp') -Raw
foreach ($marker in @('MOVEFILE_REPLACE_EXISTING', 'MOVEFILE_WRITE_THROUGH', '.corrupt.bak', 'ReloadIfChanged', 'candidate_count != 5')) {
  if ($store -notmatch [regex]::Escape($marker)) {
    throw "Settings store lost contract marker: $marker"
  }
}

$dialog = Get-Content -LiteralPath (Join-Path $repoRoot 'LeavesStoneSettings\SettingsDialog.cpp') -Raw
foreach ($marker in @('kCandidateCount', 'kTextSize', 'kOrientation', 'kAnimation', 'kDecoration', 'DrawPreview')) {
  if ($dialog -notmatch [regex]::Escape($marker)) {
    throw "Settings dialog lost approved control: $marker"
  }
}

$panel = Get-Content -LiteralPath (Join-Path $repoRoot 'WeaselUI\WeaselPanel.cpp') -Raw
foreach ($marker in @('ReloadIfChanged', '_ApplyLeavesStoneSettings', 'm_scene_renderer.DiscardDeviceResources')) {
  if ($panel -notmatch [regex]::Escape($marker)) {
    throw "Candidate UI lost settings hot-reload marker: $marker"
  }
}

$compartment = Get-Content -LiteralPath (Join-Path $repoRoot 'WeaselTSF\Compartment.cpp') -Raw
foreach ($marker in @('PlanModeSwitch', 'CompositionAction::Commit', '_EndComposition(_pEditSessionContext, false)')) {
  if ($compartment -notmatch [regex]::Escape($marker)) {
    throw "Input-state integration lost marker: $marker"
  }
}

$solution = Get-Content -LiteralPath (Join-Path $repoRoot 'weasel.sln') -Raw
if ($solution -notmatch [regex]::Escape('LeavesStoneSettings\LeavesStoneSettings.vcxproj')) {
  throw 'Settings application is not included in weasel.sln'
}

[xml](Get-Content -LiteralPath (Join-Path $repoRoot 'LeavesStoneSettings\LeavesStoneSettings.vcxproj') -Raw) | Out-Null
[xml](Get-Content -LiteralPath (Join-Path $repoRoot 'test\TestLeavesStoneUI\TestLeavesStoneUI.vcxproj') -Raw) | Out-Null

Write-Host 'TestSettingsContract: PASS'
