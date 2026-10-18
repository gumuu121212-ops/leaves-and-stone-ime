#include "SceneRenderer.h"

#include <algorithm>
#include <cmath>
#include <string>

// windows.h defines min and max as macros unless NOMINMAX is set, which turns
// every std::min/std::max below into a syntax error. Other Weasel code relies on
// those macros, so opt out only in this translation unit.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace leaves_stone {
namespace {

BYTE WithOpacity(BYTE alpha, float opacity) {
  return static_cast<BYTE>(std::clamp(opacity, 0.0f, 1.0f) * alpha);
}

Gdiplus::Color Color(BYTE alpha,
                     BYTE red,
                     BYTE green,
                     BYTE blue,
                     float opacity) {
  return Gdiplus::Color(WithOpacity(alpha, opacity), red, green, blue);
}

Gdiplus::RectF LocalRect(const CandidateScene& scene, const RectF& rect) {
  return {rect.left - scene.window_bounds.left,
          rect.top - scene.window_bounds.top, rect.Width(), rect.Height()};
}

void AddRoundedRect(Gdiplus::GraphicsPath* path,
                    const Gdiplus::RectF& rect,
                    float radius) {
  if (!path)
    return;
  const float diameter =
      std::min(radius * 2.0f, std::min(rect.Width, rect.Height));
  if (diameter <= 0.0f) {
    path->AddRectangle(rect);
    return;
  }
  path->AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
  path->AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0f,
               90.0f);
  path->AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter,
               diameter, diameter, 0.0f, 90.0f);
  path->AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0f,
               90.0f);
  path->CloseFigure();
}

void FillRoundedRect(Gdiplus::Graphics* graphics,
                     const Gdiplus::RectF& rect,
                     float radius,
                     Gdiplus::Brush* brush) {
  Gdiplus::GraphicsPath path;
  AddRoundedRect(&path, rect, radius);
  if (brush)
    graphics->FillPath(brush, &path);
}

bool DrawAsset(HDC target,
               const ThemeBitmap* bitmap,
               int x,
               int y,
               int width,
               int height,
               BYTE opacity) {
  if (!target || !bitmap || !bitmap->handle || width <= 0 || height <= 0) {
    return false;
  }
  if (opacity == 0)
    return true;
  HDC source = CreateCompatibleDC(target);
  if (!source)
    return false;
  const HGDIOBJ previous = SelectObject(source, bitmap->handle);
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, opacity, AC_SRC_ALPHA};
  const BOOL drawn = AlphaBlend(target, x, y, width, height, source, 0, 0,
                                bitmap->width, bitmap->height, blend);
  SelectObject(source, previous);
  DeleteDC(source);
  return drawn == TRUE;
}

// Same as DrawAsset but from a source rectangle, so the mascot is drawn from its
// content box instead of stretching the whole 1024 square.
bool DrawAssetCrop(HDC target,
                   const ThemeBitmap* bitmap,
                   int src_x,
                   int src_y,
                   int src_width,
                   int src_height,
                   const Gdiplus::RectF& destination,
                   BYTE opacity) {
  if (!target || !bitmap || !bitmap->handle || src_width <= 0 ||
      src_height <= 0 || destination.Width <= 0.0f ||
      destination.Height <= 0.0f) {
    return false;
  }
  if (opacity == 0)
    return true;
  HDC source = CreateCompatibleDC(target);
  if (!source)
    return false;
  const HGDIOBJ previous = SelectObject(source, bitmap->handle);
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, opacity, AC_SRC_ALPHA};
  const BOOL drawn = AlphaBlend(
      target, static_cast<int>(std::floor(destination.X)),
      static_cast<int>(std::floor(destination.Y)),
      static_cast<int>(std::ceil(destination.Width)),
      static_cast<int>(std::ceil(destination.Height)), source, src_x, src_y,
      src_width, src_height, blend);
  SelectObject(source, previous);
  DeleteDC(source);
  return drawn == TRUE;
}

// Content box shared by every mascot frame (spec: 342,450,340,188).
constexpr int kMascotContentX = 342;
constexpr int kMascotContentY = 450;
constexpr int kMascotContentWidth = 340;
constexpr int kMascotContentHeight = 188;

}  // namespace

SceneRenderer::SceneRenderer() {
  Gdiplus::GdiplusStartupInput input;
  if (Gdiplus::GdiplusStartup(&gdiplus_token_, &input, nullptr) !=
      Gdiplus::Ok) {
    gdiplus_token_ = 0;
  }
}

SceneRenderer::~SceneRenderer() {
  DiscardDeviceResources();
  if (gdiplus_token_)
    Gdiplus::GdiplusShutdown(gdiplus_token_);
}

bool SceneRenderer::EnsureDeviceResources(float dpi_scale) {
  dpi_scale = std::max(0.5f, dpi_scale);
  if (HasDeviceResources() &&
      std::fabs(resource_dpi_scale_ - dpi_scale) < 0.001f) {
    return true;
  }
  DiscardDeviceResources();
  if (!gdiplus_token_)
    return false;

  candidate_font_ = new Gdiplus::Font(L"Microsoft YaHei UI", 16.0f * dpi_scale,
                                      Gdiplus::FontStyleRegular,
                                      Gdiplus::UnitPixel);
  label_font_ = new Gdiplus::Font(L"Microsoft YaHei UI", 14.0f * dpi_scale,
                                  Gdiplus::FontStyleRegular,
                                  Gdiplus::UnitPixel);
  pinyin_font_ = new Gdiplus::Font(L"Microsoft YaHei UI", 18.0f * dpi_scale,
                                   Gdiplus::FontStyleRegular,
                                   Gdiplus::UnitPixel);
  left_format_ = new Gdiplus::StringFormat();
  if (!candidate_font_ || !label_font_ || !pinyin_font_ || !left_format_ ||
      candidate_font_->GetLastStatus() != Gdiplus::Ok ||
      label_font_->GetLastStatus() != Gdiplus::Ok ||
      pinyin_font_->GetLastStatus() != Gdiplus::Ok) {
    DiscardDeviceResources();
    return false;
  }
  left_format_->SetAlignment(Gdiplus::StringAlignmentNear);
  left_format_->SetLineAlignment(Gdiplus::StringAlignmentCenter);
  left_format_->SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
  left_format_->SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
  resource_dpi_scale_ = dpi_scale;
  ++resource_generation_;
  return true;
}

bool SceneRenderer::Render(HDC target,
                           const CandidateScene& scene,
                           const ThemeAssets& assets,
                           float opacity) {
  if (!target || scene.window_bounds.Width() <= 0.0f ||
      scene.window_bounds.Height() <= 0.0f ||
      !EnsureDeviceResources(scene.dpi_scale)) {
    return false;
  }
  opacity = std::clamp(opacity, 0.0f, 1.0f);
  const float scale = std::max(0.5f, scene.dpi_scale);
  const int width = static_cast<int>(std::ceil(scene.window_bounds.Width()));
  const int height = static_cast<int>(std::ceil(scene.window_bounds.Height()));

  {
    Gdiplus::Graphics clear(target);
    clear.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    Gdiplus::SolidBrush transparent(Gdiplus::Color(0, 0, 0, 0));
    clear.FillRectangle(&transparent, 0, 0, width, height);
  }

  const ThemeBitmap* panel = assets.Get(ThemeAssetId::Panel);
  if (!panel || !scene.has_bar) {
    // Missing compiled artwork must report failure instead of silently drawing
    // an empty window.
    return false;
  }

  if (scene.has_bar) {
    const Gdiplus::RectF bar = LocalRect(scene, scene.bar);
    // Whole-image stretch keeps the plush outline; cropping would cut the
    // rounded ends off.
    DrawAsset(target, panel, static_cast<int>(std::floor(bar.X)),
              static_cast<int>(std::floor(bar.Y)),
              static_cast<int>(std::ceil(bar.Width)),
              static_cast<int>(std::ceil(bar.Height)),
              WithOpacity(255, opacity));
  }

  if (const ThemeBitmap* mascot = assets.Get(ThemeAssetId::MascotDefault)) {
    DrawAssetCrop(target, mascot, kMascotContentX, kMascotContentY,
                  kMascotContentWidth, kMascotContentHeight,
                  LocalRect(scene, scene.mascot), WithOpacity(255, opacity));
  }

  {
    Gdiplus::Graphics graphics(target);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    // Per-pixel alpha surface: ClearType would leave coloured fringes.
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);

    if (!scene.pinyin.text.empty()) {
      const Gdiplus::RectF plaque = LocalRect(scene, scene.pinyin.bounds);
      Gdiplus::SolidBrush pinyin_brush(Color(255, 0x35, 0x2B, 0x24, opacity));
      graphics.DrawString(scene.pinyin.text.c_str(), -1, pinyin_font_, plaque,
                          left_format_, &pinyin_brush);
    }

    for (const auto& candidate : scene.candidates) {
      const Gdiplus::RectF item = LocalRect(scene, candidate.bounds);
      if (candidate.selected) {
        Gdiplus::RectF capsule = item;
        capsule.Inflate(2.0f * scale, 0.0f);
        Gdiplus::SolidBrush capsule_brush(
            Color(245, 0xD5, 0xBD, 0xA7, opacity));
        FillRoundedRect(&graphics, capsule, 14.0f * scale, &capsule_brush);
      }
      const std::wstring index_text =
          std::to_wstring(candidate.candidate_index + 1);
      Gdiplus::RectF index_rect(item.X + 6.0f * scale, item.Y,
                                16.0f * scale, item.Height);
      Gdiplus::SolidBrush index_brush(
          candidate.selected ? Color(255, 0x59, 0x4B, 0x3E, opacity)
                             : Color(255, 0xA6, 0x8D, 0x79, opacity));
      graphics.DrawString(index_text.c_str(), -1, label_font_, index_rect,
                          left_format_, &index_brush);

      Gdiplus::RectF text_rect(item.X + 24.0f * scale, item.Y,
                               item.Width - 26.0f * scale, item.Height);
      Gdiplus::SolidBrush text_brush(Color(255, 0x44, 0x37, 0x2F, opacity));
      graphics.DrawString(candidate.display_text.c_str(), -1, candidate_font_,
                          text_rect, left_format_, &text_brush);
    }

    if (scene.page_group.Width() > 0.0f && scene.controls.size() >= 2) {
      const Gdiplus::RectF group = LocalRect(scene, scene.page_group);
      Gdiplus::SolidBrush group_brush(Color(237, 0xFB, 0xF6, 0xEF, opacity));
      FillRoundedRect(&graphics, group, 14.0f * scale, &group_brush);
      Gdiplus::Pen arrow_pen(Color(255, 0x79, 0x65, 0x52, opacity),
                             1.8f * scale);
      arrow_pen.SetStartCap(Gdiplus::LineCapRound);
      arrow_pen.SetEndCap(Gdiplus::LineCapRound);
      const Gdiplus::RectF previous =
          LocalRect(scene, scene.controls[0].bounds);
      const Gdiplus::RectF next = LocalRect(scene, scene.controls[1].bounds);
      const Gdiplus::PointF left_points[3] = {
          {previous.X + previous.Width * 0.58f, previous.Y + 7.0f * scale},
          {previous.X + previous.Width * 0.42f,
           previous.Y + previous.Height * 0.5f},
          {previous.X + previous.Width * 0.58f,
           previous.Y + previous.Height - 7.0f * scale}};
      const Gdiplus::PointF right_points[3] = {
          {next.X + next.Width * 0.42f, next.Y + 7.0f * scale},
          {next.X + next.Width * 0.58f, next.Y + next.Height * 0.5f},
          {next.X + next.Width * 0.42f, next.Y + next.Height - 7.0f * scale}};
      graphics.DrawLines(&arrow_pen, left_points, 3);
      graphics.DrawLines(&arrow_pen, right_points, 3);
    }

    if (scene.paw.Width() > 0.0f) {
      if (const ThemeBitmap* paw = assets.Get(ThemeAssetId::Paw)) {
        const Gdiplus::RectF rect = LocalRect(scene, scene.paw);
        DrawAsset(target, paw, static_cast<int>(std::floor(rect.X)),
                  static_cast<int>(std::floor(rect.Y)),
                  static_cast<int>(std::ceil(rect.Width)),
                  static_cast<int>(std::ceil(rect.Height)),
                  WithOpacity(235, opacity));
      }
    }

    graphics.Flush(Gdiplus::FlushIntentionSync);
  }

  return true;
}

void SceneRenderer::DiscardDeviceResources() {
  delete candidate_font_;
  delete label_font_;
  delete pinyin_font_;
  delete left_format_;
  candidate_font_ = nullptr;
  label_font_ = nullptr;
  pinyin_font_ = nullptr;
  left_format_ = nullptr;
  resource_dpi_scale_ = 0.0f;
}

bool SceneRenderer::HasDeviceResources() const {
  return candidate_font_ && label_font_ && pinyin_font_ && left_format_;
}

size_t SceneRenderer::ResourceGeneration() const {
  return resource_generation_;
}

}  // namespace leaves_stone
