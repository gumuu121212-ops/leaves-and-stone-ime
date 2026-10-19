$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$required = @(
  'WeaselUI\LeavesStone\SceneTypes.h',
  'WeaselUI\LeavesStone\SceneLayout.h',
  'WeaselUI\LeavesStone\SceneLayout.cpp',
  'WeaselUI\LeavesStone\ThemeConfig.h',
  'WeaselUI\LeavesStone\ThemeConfig.cpp'
)

foreach ($relativePath in $required) {
  if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $relativePath))) {
    throw "Missing scene-model implementation: $relativePath"
  }
}

$testSource = Get-Content -LiteralPath (Join-Path $repoRoot 'test\TestLeavesStoneUI\SceneLayoutTests.cpp') -Raw
foreach ($case in @('0u, 1u, 5u, 7u, 9u', '1.25f', '1.5f', '2.0f', 'work area at a lower-edge caret', 'elevation', 'HitTest', 'has_visible_shared_background')) {
  if ($testSource -notmatch [regex]::Escape($case)) { throw "Scene tests lost coverage marker: $case" }
}

Write-Host 'TestSceneContract: PASS'
