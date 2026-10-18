#pragma once

#include <windows.h>

#include <array>
#include <cstddef>

struct IWICImagingFactory;

namespace leaves_stone {

enum class ThemeAssetId : size_t {
  MascotDefault = 0,
  MascotHover,
  MascotPressed,
  MascotIdle,
  MascotAvatar,
  Panel,
  Menu,
  Paw,
  IconZh,
  IconEn,
  IconJp,
  IconKr,
  IconMoon,
  IconShirt,
  IconGear,
  // Floating doggy toolbar.
  ToolbarDefault,
  ToolbarHover,
  ToolbarPressed,
  ToolbarIdle,
  ModeBackground,
  ModeGlyphZh,
  ModeGlyphEn,
  TbIconMoon,
  TbIconShirt,
  TbIconGear,
  ButtonHover,
  ButtonPressed,
  DecorHover,
  DecorPressed,
  DecorIdle,
  Count
};

struct ThemeBitmap {
  HBITMAP handle = nullptr;
  UINT width = 0;
  UINT height = 0;
};

class ThemeAssets {
 public:
  ThemeAssets() = default;
  ~ThemeAssets();

  ThemeAssets(const ThemeAssets&) = delete;
  ThemeAssets& operator=(const ThemeAssets&) = delete;

  bool Load(HMODULE module = nullptr);
  void Discard();
  const ThemeBitmap* Get(ThemeAssetId id) const;
  size_t LoadedBitmapCount() const;

 private:
  bool DecodeResource(HMODULE module,
                      int resource_id,
                      ThemeBitmap* destination);

  std::array<ThemeBitmap, static_cast<size_t>(ThemeAssetId::Count)> bitmaps_{};
  IWICImagingFactory* factory_ = nullptr;
  bool owns_com_initialization_ = false;
};

}  // namespace leaves_stone
