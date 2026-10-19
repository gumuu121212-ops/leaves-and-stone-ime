#pragma once

#include <windows.h>
#include <gdiplus.h>

#include <cstddef>

#include "SceneTypes.h"
#include "ThemeAssets.h"

namespace leaves_stone {

class SceneRenderer {
 public:
  SceneRenderer();
  ~SceneRenderer();

  SceneRenderer(const SceneRenderer&) = delete;
  SceneRenderer& operator=(const SceneRenderer&) = delete;

  CandidateScene BuildScene(const SceneInput& input);

  bool Render(HDC target,
              const CandidateScene& scene,
              const ThemeAssets& assets,
              float opacity = 1.0f);
  void DiscardDeviceResources();
  bool HasDeviceResources() const;
  size_t ResourceGeneration() const;

 private:
  bool EnsureDeviceResources(float dpi_scale);

  ULONG_PTR gdiplus_token_ = 0;
  Gdiplus::Font* candidate_font_ = nullptr;
  Gdiplus::Font* label_font_ = nullptr;
  Gdiplus::Font* pinyin_font_ = nullptr;
  Gdiplus::StringFormat* left_format_ = nullptr;
  float resource_dpi_scale_ = 0.0f;
  size_t resource_generation_ = 0;
};

}  // namespace leaves_stone
