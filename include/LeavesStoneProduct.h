#pragma once

#include <guiddef.h>

namespace leaves_stone::product {
inline constexpr char kDisplayNameA[] = "Leaves & Stone";
inline constexpr wchar_t kDisplayName[] = L"Leaves & Stone";
inline constexpr wchar_t kRegistryRoot[] = L"Software\\LeavesAndStone\\IME";
inline constexpr wchar_t kUserDataSubdir[] = L"LeavesAndStone\\Rime";
inline const GUID kTextServiceClsid = {0x6d54e957, 0xf147, 0x4dc0, {0x87, 0xa2, 0xeb, 0x29, 0x01, 0xb3, 0x2a, 0xe7}};
inline const GUID kLanguageProfileGuid = {0x3d03165f, 0x5628, 0x4c21, {0xbb, 0xbb, 0x1f, 0x2c, 0xba, 0x6f, 0x92, 0x53}};
inline const GUID kCandidateUiGuid = {0x1e99c5d6, 0x061e, 0x4e55, {0x9f, 0xae, 0xc0, 0x8d, 0x34, 0x44, 0x27, 0xc5}};
}  // namespace leaves_stone::product
