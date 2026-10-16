$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$required = @(
  'WeaselUI\LeavesStone\SceneRenderer.h',
  'WeaselUI\LeavesStone\SceneRenderer.cpp',
  'WeaselUI\LeavesStone\ThemeAssets.h',
  'WeaselUI\LeavesStone\ThemeAssets.cpp',
  'test\TestLeavesStoneUI\RendererSmokeTests.cpp',
  'resource\leaves-stone\leaf-left.png',
  'resource\leaves-stone\leaf-right.png',
  'resource\leaves-stone\building-terracotta.png',
  'resource\leaves-stone\building-ivory.png',
  'resource\leaves-stone\sky-blue.png',
  'resource\leaves-stone\architectural-lines.png'
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
foreach ($marker in @('DrawRearArtwork', 'BuildingTerracotta', 'thickness', 'DrawFrontArtwork', 'AlphaBlend')) {
  if ($renderer -notmatch [regex]::Escape($marker)) {
    throw "Renderer lost draw-order marker: $marker"
  }
}

$frontArtwork = $renderer.IndexOf('DrawFrontArtwork(target, scene, assets, opacity)')
$topmostControls = $renderer.IndexOf('Independent page controls stay above the decorative foliage')
if ($frontArtwork -lt 0 -or $topmostControls -lt 0 -or $frontArtwork -gt $topmostControls) {
  throw 'Front foliage must render after cards and before independent page controls'
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
