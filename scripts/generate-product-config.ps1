[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$configPath = Join-Path $repoRoot 'config\product.json'
$config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json

function ConvertTo-CppStringLiteral {
  param([Parameter(Mandatory)] [string]$Value)
  $Value.Replace('\', '\\').Replace('"', '\"')
}

function ConvertTo-CppGuidInitializer {
  param([Parameter(Mandatory)] [string]$Value)
  $bytes = ([guid]::Parse($Value)).ToByteArray()
  $data1 = [BitConverter]::ToUInt32($bytes, 0)
  $data2 = [BitConverter]::ToUInt16($bytes, 4)
  $data3 = [BitConverter]::ToUInt16($bytes, 6)
  $tail = ($bytes[8..15] | ForEach-Object { '0x{0:x2}' -f $_ }) -join ', '
  return "{0x$($data1.ToString('x8')), 0x$($data2.ToString('x4')), 0x$($data3.ToString('x4')), {$tail}}"
}

$displayName = ConvertTo-CppStringLiteral $config.display_name
$registryRoot = ConvertTo-CppStringLiteral $config.registry_root
$userDataSubdir = ConvertTo-CppStringLiteral $config.user_data_subdir
$registryRootWow64 = ConvertTo-CppStringLiteral ('Software\WOW6432Node\' + $config.registry_root.Substring('Software\'.Length))
$registryUpdates = ConvertTo-CppStringLiteral ($config.registry_root + '\Updates')
$textServiceClsid = ConvertTo-CppGuidInitializer $config.text_service_clsid
$languageProfileGuid = ConvertTo-CppGuidInitializer $config.language_profile_guid
$candidateUiGuid = ConvertTo-CppGuidInitializer $config.candidate_ui_guid

$header = @"
#pragma once

#include <guiddef.h>

namespace leaves_stone::product {
inline constexpr char kDisplayNameA[] = "$displayName";
inline constexpr wchar_t kDisplayName[] = L"$displayName";
inline constexpr char kRegistryRootA[] = "$registryRoot";
inline constexpr wchar_t kRegistryRoot[] = L"$registryRoot";
inline constexpr wchar_t kRegistryRootWow64[] = L"$registryRootWow64";
inline constexpr char kRegistryUpdatesA[] = "$registryUpdates";
inline constexpr wchar_t kRegistryUpdates[] = L"$registryUpdates";
inline constexpr wchar_t kUserDataSubdir[] = L"$userDataSubdir";
inline constexpr wchar_t kInstallLayoutHans[] = L"0804:{$($config.text_service_clsid)}{$($config.language_profile_guid)}";
inline constexpr wchar_t kInstallLayoutHant[] = L"0404:{$($config.text_service_clsid)}{$($config.language_profile_guid)}";
inline const GUID kTextServiceClsid = $textServiceClsid;
inline const GUID kLanguageProfileGuid = $languageProfileGuid;
inline const GUID kCandidateUiGuid = $candidateUiGuid;
}  // namespace leaves_stone::product
"@

$nsis = @"
; Generated from config/product.json. Do not edit by hand.
!define LEAVES_STONE_DISPLAY_NAME "$($config.display_name)"
!define LEAVES_STONE_PRODUCT_SLUG "$($config.product_slug)"
!define LEAVES_STONE_REGISTRY_ROOT "$($config.registry_root)"
!define LEAVES_STONE_USER_DATA_SUBDIR "$($config.user_data_subdir)"
!define LEAVES_STONE_TEXT_SERVICE_CLSID "{$($config.text_service_clsid)}"
!define LEAVES_STONE_LANGUAGE_PROFILE_GUID "{$($config.language_profile_guid)}"
!define LEAVES_STONE_CANDIDATE_UI_GUID "{$($config.candidate_ui_guid)}"
"@

$headerPath = Join-Path $repoRoot 'include\LeavesStoneProduct.h'
$nsisPath = Join-Path $repoRoot 'output\LeavesStoneProduct.nsh'
[System.IO.File]::WriteAllText($headerPath, $header, [System.Text.UTF8Encoding]::new($false))
# Normalise the generated include to LF with a trailing newline and no BOM.
# A here-string inherited CRLF from this script and dropped the final newline,
# so every regeneration used to dirty the tree right after the contract test
# ran. Deriving the bytes from one canonical string keeps runs idempotent.
$nsisNormalized = ($nsis -replace "`r`n", "`n").TrimEnd("`n") + "`n"
[System.IO.File]::WriteAllText($nsisPath, $nsisNormalized, [System.Text.UTF8Encoding]::new($false))
