#include "SceneRenderer.h"

#include <algorithm>
#include <cmath>
#include <string>

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
  const float diameter = std::min(radius * 2.0f,
                                  std::min(rect.Width, rect.Height));
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
                     Gdiplus::Brush* brush,
                     Gdiplus::Pen* pen = nullptr) {
  Gdiplus::GraphicsPath path;
  AddRoundedRect(&path, rect, radius);
  if (brush)
    graphics->FillPath(brush, &path);
  if (pen)
    graphics->DrawPath(pen, &path);
}

bool DrawAsset(HDC target,
               const ThemeBitmap* bitmap,
               int x,
               int y,
               int width,
               int height,
               BYTE opacity) {
  if (!target || !bitmap || !bitmap->handle || width <= 0 || height <= 0 ||
      opacity == 0) {
    return false;
  }
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

void DrawRearArtwork(HDC target,
                     const CandidateScene& scene,
                     const ThemeAssets& assets,
                     float opacity) {
  const int width = static_cast<int>(std::ceil(scene.window_bounds.Width()));
  const int height = static_cast<int>(std::ceil(scene.window_bounds.Height()));
  const BYTE rear = WithOpacity(155, opacity);
  DrawAsset(target, assets.Get(ThemeAssetId::SkyBlue),
            static_cast<int>(width * 0.10f), 1,
            static_cast<int>(width * 0.22f), height - 2, rear);
  DrawAsset(target, assets.Get(ThemeAssetId::BuildingTerracotta), 0,
            static_cast<int>(height * 0.08f),
            std::max(28, static_cast<int>(width * 0.10f)),
            static_cast<int>(height * 0.82f), WithOpacity(105, opacity));
  DrawAsset(target, assets.Get(ThemeAssetId::BuildingIvory),
            static_cast<int>(width * 0.88f),
            static_cast<int>(height * 0.05f),
            std::max(26, static_cast<int>(width * 0.12f)),
            static_cast<int>(height * 0.88f), WithOpacity(112, opacity));
  DrawAsset(target, assets.Get(ThemeAssetId::ArchitecturalLines),
            static_cast<int>(width * 0.28f), 0,
            static_cast<int>(width * 0.64f), height,
            WithOpacity(72, opacity));
}

void DrawFrontArtwork(HDC target,
                      const CandidateScene& scene,
                      const ThemeAssets& assets,
                      float opacity) {
  if (scene.decorations.empty())
    return;
  const int width = static_cast<int>(std::ceil(scene.window_bounds.Width()));
  const int height = static_cast<int>(std::ceil(scene.window_bounds.Height()));
  const int leaf_height = std::max(18, static_cast<int>(height * 0.38f));
  const int leaf_width = std::max(22, static_cast<int>(leaf_height * 1.08f));
  DrawAsset(target, assets.Get(ThemeAssetId::LeafLeft), 2, 0, leaf_width,
            leaf_height, WithOpacity(230, opacity));
  DrawAsset(target, assets.Get(ThemeAssetId::LeafRight),
            width - leaf_width - 2, 0, leaf_width, leaf_height,
            WithOpacity(220, opacity));
}

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

  candidate_font_ = new Gdiplus::Font(L"Microsoft YaHei UI", 13.0f * dpi_scale,
                                      Gdiplus::FontStyleRegular,
                                      Gdiplus::UnitPixel);
  label_font_ = new Gdiplus::Font(L"Microsoft YaHei UI", 8.0f * dpi_scale,
                                  Gdiplus::FontStyleBold,
                                  Gdiplus::UnitPixel);
  pinyin_font_ = new Gdiplus::Font(L"Microsoft YaHei UI", 11.0f * dpi_scale,
                                   Gdiplus::FontStyleRegular,
                                   Gdiplus::UnitPixel);
  centered_format_ = new Gdiplus::StringFormat();
  if (!candidate_font_ || !label_font_ || !pinyin_font_ || !centered_format_ ||
      candidate_font_->GetLastStatus() != Gdiplus::Ok ||
      label_font_->GetLastStatus() != Gdiplus::Ok ||
      pinyin_font_->GetLastStatus() != Gdiplus::Ok) {
    DiscardDeviceResources();
    return false;
  }
  centered_format_->SetAlignment(Gdiplus::StringAlignmentCenter);
  centered_format_->SetLineAlignment(Gdiplus::StringAlignmentCenter);
  centered_format_->SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
  centered_format_->SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
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
  DrawRearArtwork(target, scene, assets, opacity);

  Gdiplus::Graphics graphics(target);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
  graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);

  if (!scene.pinyin.text.empty()) {
    Gdiplus::RectF plaque = LocalRect(scene, scene.pinyin.bounds);
    Gdiplus::RectF shadow = plaque;
    shadow.X += 1.5f * scale;
    shadow.Y += 3.0f * scale;
    Gdiplus::SolidBrush shadow_brush(Color(54, 39, 48, 38, opacity));
    FillRoundedRect(&graphics, shadow, 10.0f * scale, &shadow_brush);
    Gdiplus::SolidBrush plaque_brush(Color(248, 185, 222, 235, opacity));
    Gdiplus::Pen plaque_edge(Color(150, 91, 133, 143, opacity), 1.0f * scale);
    FillRoundedRect(&graphics, plaque, 10.0f * scale, &plaque_brush,
                    &plaque_edge);
    Gdiplus::SolidBrush text_brush(Color(255, 45, 55, 49, opacity));
    graphics.DrawString(scene.pinyin.text.c_str(), -1, pinyin_font_, plaque,
                        centered_format_, &text_brush);
  }

  const Gdiplus::Color thickness_colors[] = {
      Color(255, 176, 91, 66, opacity), Color(255, 80, 111, 80, opacity),
      Color(255, 105, 151, 167, opacity)};
  for (size_t index = 0; index < scene.candidates.size(); ++index) {
    const auto& candidate = scene.candidates[index];
    const Gdiplus::RectF face = LocalRect(scene, candidate.bounds);
    const float depth = (candidate.selected ? 6.0f : 4.0f) * scale;
    const float radius = 10.0f * scale;

    for (int ring = 3; ring >= 1; --ring) {
      Gdiplus::RectF shadow = face;
      shadow.X += (1.0f + ring * 0.5f) * scale;
      shadow.Y += (depth + ring * 1.5f) * scale;
      Gdiplus::SolidBrush shadow_brush(
          Color(static_cast<BYTE>(18 + ring * 7), 42, 48, 38, opacity));
      FillRoundedRect(&graphics, shadow, radius + ring * scale, &shadow_brush);
    }

    Gdiplus::RectF thickness = face;
    thickness.Y += depth;
    Gdiplus::SolidBrush thickness_brush(thickness_colors[index % 3]);
    FillRoundedRect(&graphics, thickness, radius, &thickness_brush);

    const Gdiplus::Color face_color =
        candidate.selected ? Color(255, 224, 239, 235, opacity)
                           : Color(255, 249, 244, 230, opacity);
    Gdiplus::SolidBrush face_brush(face_color);
    Gdiplus::Pen paper_edge(Color(175, 198, 188, 163, opacity),
                            0.8f * scale);
    FillRoundedRect(&graphics, face, radius, &face_brush, &paper_edge);

    Gdiplus::RectF highlight = face;
    highlight.Inflate(-2.0f * scale, -2.0f * scale);
    highlight.Height = std::max(1.0f, 2.0f * scale);
    Gdiplus::SolidBrush highlight_brush(Color(120, 255, 255, 250, opacity));
    FillRoundedRect(&graphics, highlight, 2.0f * scale, &highlight_brush);

    Gdiplus::RectF label(face.X + 7.0f * scale,
                         face.Y + (face.Height - 18.0f * scale) / 2.0f,
                         18.0f * scale, 18.0f * scale);
    Gdiplus::SolidBrush label_brush(
        candidate.selected ? Color(255, 64, 95, 70, opacity)
                           : Color(236, 184, 101, 74, opacity));
    graphics.FillEllipse(&label_brush, label);
    const std::wstring label_text = std::to_wstring(candidate.candidate_index + 1);
    Gdiplus::SolidBrush label_text_brush(Color(255, 255, 250, 237, opacity));
    graphics.DrawString(label_text.c_str(), -1, label_font_, label,
                        centered_format_, &label_text_brush);

    Gdiplus::RectF text_rect(face.X + 29.0f * scale, face.Y,
                             face.Width - 35.0f * scale, face.Height);
    Gdiplus::SolidBrush candidate_text(Color(255, 43, 52, 44, opacity));
    graphics.DrawString(candidate.display_text.c_str(), -1, candidate_font_,
                        text_rect, centered_format_, &candidate_text);
  }

  for (const auto& control : scene.controls) {
    const Gdiplus::RectF circle = LocalRect(scene, control.bounds);
    Gdiplus::RectF shadow = circle;
    shadow.Y += 3.0f * scale;
    Gdiplus::SolidBrush shadow_brush(Color(45, 40, 47, 38, opacity));
    graphics.FillEllipse(&shadow_brush, shadow);
    Gdiplus::SolidBrush circle_brush(
        control.enabled ? Color(250, 247, 240, 220, opacity)
                        : Color(185, 230, 226, 211, opacity));
    Gdiplus::Pen circle_edge(Color(165, 113, 121, 96, opacity), 1.0f * scale);
    graphics.FillEllipse(&circle_brush, circle);
    graphics.DrawEllipse(&circle_edge, circle);

    const float direction =
        control.kind == ControlKind::PreviousPage ? -1.0f : 1.0f;
    const float center_x = circle.X + circle.Width / 2.0f;
    const float center_y = circle.Y + circle.Height / 2.0f;
    Gdiplus::Pen arrow(
        control.enabled ? Color(255, 62, 78, 61, opacity)
                        : Color(150, 131, 135, 121, opacity),
        2.0f * scale);
    arrow.SetStartCap(Gdiplus::LineCapRound);
    arrow.SetEndCap(Gdiplus::LineCapRound);
    const Gdiplus::PointF points[3] = {
        {center_x - direction * 3.0f * scale,
         center_y - 4.0f * scale},
        {center_x + direction * 2.0f * scale, center_y},
        {center_x - direction * 3.0f * scale,
         center_y + 4.0f * scale}};
    graphics.DrawLines(&arrow, points, 3);
  }

  graphics.Flush(Gdiplus::FlushIntentionSync);
  DrawFrontArtwork(target, scene, assets, opacity);
  return true;
}

void SceneRenderer::DiscardDeviceResources() {
  delete candidate_font_;
  delete label_font_;
  delete pinyin_font_;
  delete centered_format_;
  candidate_font_ = nullptr;
  label_font_ = nullptr;
  pinyin_font_ = nullptr;
  centered_format_ = nullptr;
  resource_dpi_scale_ = 0.0f;
}

bool SceneRenderer::HasDeviceResources() const {
  return candidate_font_ && label_font_ && pinyin_font_ && centered_format_;
}

size_t SceneRenderer::ResourceGeneration() const {
  return resource_generation_;
}

}  // namespace leaves_stone
