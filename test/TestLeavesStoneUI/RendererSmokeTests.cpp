#include <windows.h>

#include <cmath>
#include <iostream>
#include <string>

#include "WeaselUI/LeavesStone/SceneLayout.h"
#include "WeaselUI/LeavesStone/SceneRenderer.h"
#include "WeaselUI/LeavesStone/ThemeAssets.h"
#include "WeaselUI/DoggyToolbar.h"

namespace {
int failures = 0;

void Check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

leaves_stone::CandidateScene MakeScene() {
  leaves_stone::SceneInput input;
  input.anchor = {80.0f, 80.0f, 82.0f, 106.0f};
  input.work_area = {0.0f, 0.0f, 1600.0f, 900.0f};
  input.preedit = L"jintian";
  input.requested_candidate_count = 5;
  input.selected_index = 1;
  input.has_previous_page = true;
  input.has_next_page = true;
  input.candidates = {{L"\u4eca\u5929", L""}, {L"\u5929\u6c14", L""},
                      {L"\u5f88\u597d", L""}, {L"\u6211\u4eec", L""},
                      {L"\u51fa\u53d1", L""}};
  return leaves_stone::BuildCandidateScene(input);
}

struct DibTarget {
  HDC dc = nullptr;
  HBITMAP bitmap = nullptr;
  HGDIOBJ old_bitmap = nullptr;
  void* pixels = nullptr;

  DibTarget(int width, int height) {
    dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    old_bitmap = SelectObject(dc, bitmap);
  }

  ~DibTarget() {
    if (dc && old_bitmap)
      SelectObject(dc, old_bitmap);
    if (bitmap)
      DeleteObject(bitmap);
    if (dc)
      DeleteDC(dc);
  }
};

void TestFirstFrameWithoutAnimationTimer() {
  leaves_stone::ThemeAssets assets;
  Check(assets.Load(GetModuleHandleW(nullptr)),
        "the first candidate frame must load the real artwork");
  leaves_stone::SceneRenderer renderer;
  const auto scene = MakeScene();
  const int width = static_cast<int>(scene.window_bounds.Width());
  const int height = static_cast<int>(scene.window_bounds.Height());
  DibTarget target(width, height);
  if (!target.pixels) {
    Check(false, "the first-frame test must create a real pixel buffer");
    return;
  }
  // The host may keep processing text without dispatching WM_TIMER. Render
  // the very first frame, with no warm-up frame and no message pumping.
  Check(renderer.Render(target.dc, scene, assets,
                        leaves_stone::CandidateAppearanceOpacity(0.0f)),
        "the first candidate frame must render before any timer message");
  const auto* pixels = static_cast<const unsigned int*>(target.pixels);
  size_t visible_pixels = 0;
  for (int i = 0; i < width * height; ++i)
    if ((pixels[i] >> 24) >= 200)
      ++visible_pixels;
  Check(visible_pixels > static_cast<size_t>(width * height / 5),
        "the first frame must contain legible opaque artwork without WM_TIMER");
  Check(leaves_stone::CandidateAppearanceOpacity(5000.0f) == 1.0f,
        "a delayed repaint must finish the fade without a timer callback");
  Check(leaves_stone::CandidateAppearanceOpacity(-1.0f) >= 0.9f &&
            leaves_stone::CandidateAppearanceOpacity(16.0f) >= 0.9f,
        "early animation frames must never hide the candidate surface");
}

void TestCompleteSingleLinePhrases() {
  leaves_stone::SceneRenderer renderer;
  for (float scale : {1.0f, 1.25f, 1.5f, 1.7f, 2.0f}) {
    leaves_stone::SceneInput input;
    input.anchor = {80.0f, 120.0f, 82.0f, 146.0f};
    input.work_area = {0.0f, 0.0f, 4000.0f, 2000.0f};
    input.dpi_scale = scale;
    input.candidates = {{L"正常输入的时候", L""}};
    const auto scene = renderer.BuildScene(input);
    const auto& card = scene.candidates.front().bounds;
    DibTarget target(1000, 1000);
    Gdiplus::Graphics graphics(target.dc);
    Gdiplus::Font font(L"Microsoft YaHei UI", 16.0f * scale,
                      Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::StringFormat format;
    const Gdiplus::RectF area(0, 0, card.Width() - 26.0f * scale,
                             card.Height());
    Gdiplus::RectF measured;
    INT characters = 0, lines = 0;
    graphics.MeasureString(input.candidates[0].text.c_str(), -1, &font, area,
                            &format, &measured, &characters, &lines);
    Check(characters == 7 && lines == 1,
          "all seven characters must fit on one line with the actual font");
  }
}

void TestCompleteWrappedCandidate() {
  leaves_stone::SceneInput input;
  input.anchor = {80.0f, 120.0f, 82.0f, 146.0f};
  input.work_area = {0.0f, 0.0f, 800.0f, 1000.0f};
  input.candidates = {{std::wstring(100, L'好'), L""}};
  leaves_stone::SceneRenderer renderer;
  const auto scene = renderer.BuildScene(input);
  const auto& card = scene.candidates.front().bounds;
  Check(card.Height() > 31.0f,
        "a screen-wide long candidate needs room for multiple text lines");

  leaves_stone::ThemeAssets assets;
  Check(assets.Load(GetModuleHandleW(nullptr)),
        "wrapped-text rendering must load the real compiled artwork");
  const int width = static_cast<int>(scene.window_bounds.Width());
  DibTarget target(width, static_cast<int>(scene.window_bounds.Height()));
  Check(renderer.Render(target.dc, scene, assets),
        "wrapped-text rendering must draw the full scene");
  if (!target.pixels || card.Height() <= 31.0f)
    return;

  // The old single-line renderer centres its one line halfway down the card.
  // The final line of this complete phrase must instead leave actual text ink
  // near the bottom, beyond the index and away from the cushion's edges.
  int dark_pixels = 0;
  const auto* pixels = static_cast<const unsigned int*>(target.pixels);
  const int left = static_cast<int>(card.left - scene.window_bounds.left) + 30;
  const int right = static_cast<int>(card.right - scene.window_bounds.left) - 8;
  const int top = static_cast<int>(card.bottom - scene.window_bounds.top) - 16;
  const int bottom = static_cast<int>(card.bottom - scene.window_bounds.top) - 4;
  for (int y = top; y < bottom; ++y) {
    for (int x = left; x < right; ++x) {
      const auto pixel = pixels[y * width + x];
      if ((pixel >> 24) > 128 && ((pixel >> 16) & 255) < 120 &&
          ((pixel >> 8) & 255) < 110 && (pixel & 255) < 100)
        ++dark_pixels;
    }
  }
  Check(dark_pixels > 10,
        "the last wrapped text line must be painted instead of clipped away");
}

void TestResourceLifecycle() {
  leaves_stone::ThemeAssets assets;
  Check(assets.Load(GetModuleHandleW(nullptr)),
        "all compiled theme assets must decode through WIC");
  Check(assets.LoadedBitmapCount() == 34,
        "the runtime skin must expose all thirty-six decoded layers");

  const auto scene = MakeScene();
  DibTarget target(static_cast<int>(scene.window_bounds.Width()),
                   static_cast<int>(scene.window_bounds.Height()));
  Check(target.dc != nullptr && target.bitmap != nullptr,
        "renderer smoke test must create an ARGB target");

  {
    leaves_stone::SceneRenderer missing_asset_renderer;
    leaves_stone::ThemeAssets missing_assets;
    Check(!missing_asset_renderer.Render(target.dc, scene, missing_assets, 1.0f),
          "renderer must report missing compiled artwork instead of silently "
          "succeeding");
  }
  leaves_stone::SceneRenderer renderer;
  // GDI+ creates its hook window and caches font data the first time it draws
  // text, so the leak baseline has to be taken after one warm frame. Measuring
  // from before the first frame reported that one-time work as a leak.
  Check(renderer.Render(target.dc, scene, assets, 1.0f),
        "renderer must draw one warm-up frame before the baseline is measured");
  const size_t generation_before = renderer.ResourceGeneration();
  const DWORD gdi_before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
  const DWORD user_before = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
  for (int cycle = 0; cycle < 1000; ++cycle) {
    renderer.DiscardDeviceResources();
    Check(renderer.Render(target.dc, scene, assets, 1.0f),
          "renderer must recreate resources after a forced discard");
  }
  Check(renderer.ResourceGeneration() == generation_before + 1000,
        "each forced discard must result in one resource recreation");
  renderer.DiscardDeviceResources();
  Check(!renderer.HasDeviceResources(),
        "renderer discard must release every device resource");
  const DWORD gdi_after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
  const DWORD user_after = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
  Check(gdi_after <= gdi_before + 2,
        "repeated render/discard must keep the GDI handle count stable (" +
            std::to_string(gdi_before) + " -> " + std::to_string(gdi_after) +
            ")");
  Check(user_after <= user_before + 1,
        "repeated render/discard must keep the USER handle count stable (" +
            std::to_string(user_before) + " -> " + std::to_string(user_after) +
            ")");

  assets.Discard();
  Check(assets.LoadedBitmapCount() == 0,
        "asset discard must release every decoded bitmap");
  Check(assets.Load(GetModuleHandleW(nullptr)),
        "theme assets must reload after a forced discard");
}

void TestSingleEnglishCandidate() {
  leaves_stone::ThemeAssets assets;
  Check(assets.Load(GetModuleHandleW(nullptr)),
        "English candidate test must load the real artwork");
  leaves_stone::SceneRenderer renderer;
  for (float scale : {0.85f, 1.0f, 1.25f, 1.7f, 2.0f}) {
    leaves_stone::SceneInput input;
    input.anchor = {400.0f, 300.0f, 402.0f, 326.0f};
    input.work_area = {0.0f, 0.0f, 1920.0f, 1080.0f};
    input.dpi_scale = scale;
    input.selected_index = 0;
    input.candidates = {{L"iPad", L""}};
    const auto scene = renderer.BuildScene(input);
    Check(scene.candidates.size() == 1 && scene.candidates[0].selected &&
              scene.candidates[0].display_text == L"iPad",
          "the only English candidate must be complete and selected");
    const auto& card = scene.candidates[0].bounds;
    const auto hit = leaves_stone::HitTest(
        scene, {(card.left + card.right) / 2, (card.top + card.bottom) / 2});
    Check(hit && hit->kind == leaves_stone::HitKind::Candidate &&
              hit->candidate_index == 0,
          "the English capsule must retain engine candidate index zero");
    const int width = static_cast<int>(std::ceil(scene.window_bounds.Width()));
    DibTarget target(width,
                     static_cast<int>(std::ceil(scene.window_bounds.Height())));
    Check(renderer.Render(target.dc, scene, assets),
          "the single English candidate must render through production code");
    if (!target.pixels)
      continue;
    const auto* pixels = static_cast<const unsigned int*>(target.pixels);
    int ink = 0;
    for (int y = static_cast<int>(card.top - scene.window_bounds.top);
         y < static_cast<int>(card.bottom - scene.window_bounds.top); ++y) {
      // Skip the numeric label: verify ink from the Latin word itself.
      for (int x = static_cast<int>(card.left - scene.window_bounds.left +
                                    24.0f * scale);
           x < static_cast<int>(card.right - scene.window_bounds.left); ++x) {
        const auto pixel = pixels[y * width + x];
        if ((pixel >> 24) > 128 && ((pixel >> 16) & 255) < 120 &&
            ((pixel >> 8) & 255) < 110 && (pixel & 255) < 100)
          ++ink;
      }
    }
    Check(ink > 10, "iPad must leave visible text ink inside its capsule");
  }
}

void TestHitRouting() {
  auto scene = MakeScene();
  const auto& candidate = scene.candidates[3].bounds;
  auto hit = leaves_stone::HitTest(
      scene, {(candidate.left + candidate.right) / 2.0f,
              (candidate.top + candidate.bottom) / 2.0f});
  Check(hit && hit->kind == leaves_stone::HitKind::Candidate &&
            hit->candidate_index == 3,
        "candidate clicks must preserve the visible candidate index");

  for (const auto& control : scene.controls) {
    hit = leaves_stone::HitTest(
        scene, {(control.bounds.left + control.bounds.right) / 2.0f,
                (control.bounds.top + control.bounds.bottom) / 2.0f});
    Check(hit &&
              ((control.kind == leaves_stone::ControlKind::PreviousPage &&
                hit->kind == leaves_stone::HitKind::PreviousPage) ||
               (control.kind == leaves_stone::ControlKind::NextPage &&
                hit->kind == leaves_stone::HitKind::NextPage)),
          "enabled page controls must route to their semantic action");
  }

  scene.controls.front().enabled = false;
  const auto& disabled = scene.controls.front().bounds;
  Check(!leaves_stone::HitTest(
            scene, {(disabled.left + disabled.right) / 2.0f,
                    (disabled.top + disabled.bottom) / 2.0f}),
        "disabled page controls must be click-through");

  const auto& left = scene.candidates[0].bounds;
  const auto& right = scene.candidates[1].bounds;
  Check(!leaves_stone::HitTest(
            scene, {(left.right + right.left) / 2.0f,
                    (left.top + left.bottom) / 2.0f}),
        "transparent card gaps must never capture input");
}

void TestAppearanceAnimationPolicy() {
  leaves_stone::ThemeConfig theme;
  Check(leaves_stone::ShouldAnimateAppearance(false, true, theme),
        "a newly visible candidate scene must animate once");
  Check(!leaves_stone::ShouldAnimateAppearance(true, true, theme),
        "keystrokes and highlight changes must not restart appearance animation");
  Check(!leaves_stone::ShouldAnimateAppearance(false, false, theme),
        "a hidden candidate scene must not animate");
  theme.animation_enabled = false;
  Check(!leaves_stone::ShouldAnimateAppearance(false, true, theme),
        "disabled animation must show the scene in one frame");
}
}  // namespace

int RunRendererSmokeTests() {
  const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  TestFirstFrameWithoutAnimationTimer();
  TestResourceLifecycle();
  TestHitRouting();
  TestAppearanceAnimationPolicy();
  TestCompleteWrappedCandidate();
  TestCompleteSingleLinePhrases();
  TestSingleEnglishCandidate();
  if (SUCCEEDED(initialized))
    CoUninitialize();
  return failures;
}
