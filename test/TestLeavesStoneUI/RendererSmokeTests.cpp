#include <windows.h>

#include <iostream>
#include <string>

#include "WeaselUI/LeavesStone/SceneLayout.h"
#include "WeaselUI/LeavesStone/SceneRenderer.h"
#include "WeaselUI/LeavesStone/ThemeAssets.h"

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

  DibTarget(int width, int height) {
    dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
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

void TestResourceLifecycle() {
  leaves_stone::ThemeAssets assets;
  Check(assets.Load(GetModuleHandleW(nullptr)),
        "all compiled theme assets must decode through WIC");
  Check(assets.LoadedBitmapCount() == 34,
        "the runtime skin must expose all fifteen decoded layers");

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
  TestResourceLifecycle();
  TestHitRouting();
  TestAppearanceAnimationPolicy();
  if (SUCCEEDED(initialized))
    CoUninitialize();
  return failures;
}
