$ErrorActionPreference = 'Stop'

$dataRoot = Join-Path (Split-Path -Parent $PSScriptRoot) 'output\data'
$required = @(
  'default.yaml',
  'leaves_stone.schema.yaml',
  'leaves_stone.dict.yaml',
  'leaves_stone_theme.yaml',
  'weasel.yaml'
)

$content = @{}
foreach ($name in $required) {
  $path = Join-Path $dataRoot $name
  if (-not (Test-Path -LiteralPath $path)) { throw "Missing packaged config: $name" }
  $content[$name] = Get-Content -LiteralPath $path -Raw
}

if (($content['default.yaml'] | Select-String -Pattern '(?m)^\s*-\s*schema:\s*leaves_stone\s*$' -AllMatches).Matches.Count -ne 1) {
  throw 'default.yaml must expose only the leaves_stone schema.'
}
if ($content['leaves_stone.schema.yaml'] -notmatch 'schema_id:\s*leaves_stone') { throw 'Missing leaves_stone schema id.' }
if ($content['leaves_stone.schema.yaml'] -notmatch 'name:\s*ascii_mode') { throw 'Missing English ASCII mode.' }
if ($content['leaves_stone.schema.yaml'] -notmatch 'name:\s*simplification') { throw 'Missing Simplified Chinese conversion.' }
if ($content['leaves_stone.schema.yaml'] -notmatch 'page_size:\s*5') { throw 'Candidate page size must default to 5.' }
if ($content['leaves_stone_theme.yaml'] -notmatch 'allowed_counts:\s*\[\s*5\s*,\s*7\s*,\s*9\s*\]') { throw 'Allowed candidate counts must be 5, 7, and 9.' }
if ($content['leaves_stone_theme.yaml'] -notmatch 'invalid_count_fallback:\s*5') { throw 'Invalid candidate counts must fall back to 5.' }
if ($content['leaves_stone_theme.yaml'] -notmatch 'orientation:\s*horizontal') { throw 'Theme must default to horizontal.' }
if ($content['leaves_stone_theme.yaml'] -notmatch 'animation_enabled:\s*true') { throw 'Animation must default on.' }
if ($content['leaves_stone_theme.yaml'] -notmatch 'decoration_enabled:\s*true') { throw 'Decoration must default on.' }
if ($content['weasel.yaml'] -notmatch 'horizontal:\s*true') { throw 'Host layout must be horizontal.' }
if ($content['weasel.yaml'] -notmatch 'inline_preedit:\s*false') { throw 'Inline preedit must be off.' }

$allContent = ($content.Values -join "`n")
foreach ($forbidden in @('http://', 'https://', 'network:', 'update_url:', 'telemetry:', 'cloud:')) {
  if ($allContent.IndexOf($forbidden, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
    throw "Packaged config contains forbidden setting: $forbidden"
  }
}

Write-Host 'TestConfigContract: PASS'
