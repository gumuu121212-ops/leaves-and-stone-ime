$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$required = @(
  'WeaselUI\LeavesStone\SceneRenderer.h',
  'WeaselUI\LeavesStone\SceneRenderer.cpp',
  'WeaselUI\LeavesStone\ThemeAssets.h',
  'WeaselUI\LeavesStone\ThemeAssets.cpp',
  'test\TestLeavesStoneUI\RendererSmokeTests.cpp',
  'resource\doggy\mascot-default.png',
  'resource\doggy\mascot-hover.png',
  'resource\doggy\mascot-pressed.png',
  'resource\doggy\mascot-idle.png',
  'resource\doggy\mascot-avatar.png',
  'resource\doggy\textures\panel-plush.png',
  'resource\doggy\textures\menu-plush.png',
  'resource\doggy\icons\icon-paw.png',
  'resource\doggy\icons\icon-zh.png',
  'resource\doggy\icons\icon-en.png',
  'resource\doggy\icons\icon-jp.png',
  'resource\doggy\icons\icon-kr.png',
  'resource\doggy\spec.md'
)

foreach ($relativePath in $required) {
  $path = Join-Path $repoRoot $relativePath
  if (-not (Test-Path -LiteralPath $path)) {
    throw "Missing renderer implementation: $relativePath"
  }
  if ((Get-Item -LiteralPath $path).Length -eq 0) {
    throw "Renderer file is empty: $relativePath"
  }
}

$renderer = Get-Content -LiteralPath (Join-Path $repoRoot 'WeaselUI\LeavesStone\SceneRenderer.cpp') -Raw
foreach ($marker in @('DrawAssetCrop', 'MascotDefault', 'page_group', 'AlphaBlend')) {
  if ($renderer -notmatch [regex]::Escape($marker)) {
    throw "Renderer lost draw marker: $marker"
  }
}

$tests = Get-Content -LiteralPath (Join-Path $repoRoot 'test\TestLeavesStoneUI\RendererSmokeTests.cpp') -Raw
foreach ($marker in @('1000', 'DiscardDeviceResources', 'transparent card gaps', 'disabled page controls', 'missing compiled artwork')) {
  if ($tests -notmatch [regex]::Escape($marker)) {
    throw "Renderer tests lost coverage marker: $marker"
  }
}

foreach ($marker in @('ShouldAnimateAppearance(false, true', 'ShouldAnimateAppearance(true, true', 'animation_enabled = false')) {
  if ($tests -notmatch [regex]::Escape($marker)) {
    throw "Renderer tests lost animation-policy marker: $marker"
  }
}

Write-Host 'TestRendererContract: PASS'
