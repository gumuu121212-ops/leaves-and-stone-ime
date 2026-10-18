#include "DoggyToolbar.h"

#include <algorithm>
#include <cmath>

#include <gdiplus.h>
#include <windowsx.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace leaves_stone {
namespace {

// GDI+ must be started once per process. The server hosts this window and has
// no other GDI+ user, so the toolbar owns the lifetime.
void EnsureGdiplus() {
  static ULONG_PTR token = []() -> ULONG_PTR {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR value = 0;
    Gdiplus::GdiplusStartup(&value, &input, nullptr);
    return value;
  }();
  (void)token;
}

constexpr wchar_t kWindowClass[] = L"LeavesStoneDoggyToolbar";

// Compact strip sized for the taskbar row, in dip.
constexpr int kWidthDip = 152;
constexpr int kHeightDip = 36;
constexpr int kBottomMarginDip = 2;
// Leave the clock and tray icons where they are.
constexpr int kTrayAllowanceDip = 190;

struct ButtonSlot {
  RECT rect;  // dip, relative to the window
  ToolbarAction action;
};

constexpr ButtonSlot kButtons[] = {
    {{36, 2, 64, 34}, ToolbarAction::ToggleMode},
    {{66, 2, 94, 34}, ToolbarAction::ToggleRest},
    {{96, 2, 122, 34}, ToolbarAction::OpenSkinPicker},
    {{122, 2, 150, 34}, ToolbarAction::OpenSettings},
};

constexpr RECT kAvatarRect = {3, 3, 33, 33};
constexpr RECT kModeRect = {38, 6, 62, 30};

Gdiplus::Color Plush(BYTE alpha) {
  return Gdiplus::Color(alpha, 0xF7, 0xF0, 0xE7);
}

Gdiplus::Color Bark(BYTE alpha) {
  return Gdiplus::Color(alpha, 0x79, 0x65, 0x52);
}

Gdiplus::Color Capsule(BYTE alpha) {
  return Gdiplus::Color(alpha, 0xD5, 0xBD, 0xA7);
}

void AddRoundRect(Gdiplus::GraphicsPath* path,
                  const Gdiplus::RectF& rect,
                  float radius) {
  const float d = std::min(radius * 2.0f, std::min(rect.Width, rect.Height));
  path->AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
  path->AddArc(rect.GetRight() - d, rect.Y, d, d, 270.0f, 90.0f);
  path->AddArc(rect.GetRight() - d, rect.GetBottom() - d, d, d, 0.0f, 90.0f);
  path->AddArc(rect.X, rect.GetBottom() - d, d, d, 90.0f, 90.0f);
  path->CloseFigure();
}

bool DrawBitmap(HDC target,
                const ThemeBitmap* bitmap,
                const RECT& destination,
                BYTE opacity) {
  if (!target || !bitmap || !bitmap->handle)
    return false;
  const int width = destination.right - destination.left;
  const int height = destination.bottom - destination.top;
  if (width <= 0 || height <= 0 || opacity == 0)
    return true;
  HDC source = CreateCompatibleDC(target);
  if (!source)
    return false;
  const HGDIOBJ previous = SelectObject(source, bitmap->handle);
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, opacity, AC_SRC_ALPHA};
  const BOOL ok = AlphaBlend(target, destination.left, destination.top, width,
                             height, source, 0, 0, bitmap->width,
                             bitmap->height, blend);
  SelectObject(source, previous);
  DeleteDC(source);
  return ok == TRUE;
}

}  // namespace

DoggyToolbar::DoggyToolbar(HINSTANCE instance) : instance_(instance) {}

DoggyToolbar::~DoggyToolbar() {
  Destroy();
}

bool DoggyToolbar::Create(std::function<void(ToolbarAction)> handler) {
  handler_ = std::move(handler);
  if (!assets_.Load())
    return false;

  WNDCLASSEXW wc = {sizeof(wc)};
  wc.lpfnWndProc = &DoggyToolbar::WindowProc;
  wc.hInstance = instance_;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = kWindowClass;
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return false;

  ReloadMetrics();
  hwnd_ = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
      kWindowClass, L"Leaves & Stone", WS_POPUP, 0, 0,
      window_rect_.right - window_rect_.left,
      window_rect_.bottom - window_rect_.top, nullptr, nullptr, instance_,
      this);
  if (!hwnd_)
    return false;

  ApplyPlacement();
  Render();
  ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
  return true;
}

void DoggyToolbar::Destroy() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  assets_.Discard();
}

void DoggyToolbar::SetAsciiMode(bool ascii) {
  if (ascii_mode_ == ascii)
    return;
  ascii_mode_ = ascii;
  Render();
}

void DoggyToolbar::ReloadMetrics() {
  HDC screen = GetDC(nullptr);
  const int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
  if (screen)
    ReleaseDC(nullptr, screen);
  scale_ = static_cast<float>(dpi) / 96.0f;
  const int width = static_cast<int>(std::lround(kWidthDip * scale_));
  const int height = static_cast<int>(std::lround(kHeightDip * scale_));
  window_rect_ = {0, 0, width, height};
}

void DoggyToolbar::ApplyPlacement() {
  const int width = window_rect_.right - window_rect_.left;
  const int height = window_rect_.bottom - window_rect_.top;
  const int right = GetSystemMetrics(SM_CXSCREEN);
  const int bottom = GetSystemMetrics(SM_CYSCREEN);
  // Inside the taskbar strip, left of the clock and tray.
  const int x = right -
                static_cast<int>(std::lround(kTrayAllowanceDip * scale_)) -
                width;
  const int y = bottom - height -
                static_cast<int>(std::lround(kBottomMarginDip * scale_));
  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  window_rect_ = {x, y, x + width, y + height};
}

RECT DoggyToolbar::DipRect(const RECT& dip) const {
  RECT rect;
  rect.left = static_cast<LONG>(std::lround(dip.left * scale_));
  rect.top = static_cast<LONG>(std::lround(dip.top * scale_));
  rect.right = static_cast<LONG>(std::lround(dip.right * scale_));
  rect.bottom = static_cast<LONG>(std::lround(dip.bottom * scale_));
  return rect;
}

DoggyToolbar::Hot DoggyToolbar::HitTest(int x, int y) const {
  const struct {
    size_t index;
    Hot hot;
  } map[] = {{0, Hot::Mode}, {1, Hot::Rest}, {2, Hot::Skin}, {3, Hot::Settings}};
  for (const auto& entry : map) {
    const RECT rect = DipRect(kButtons[entry.index].rect);
    if (x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom)
      return entry.hot;
  }
  return Hot::None;
}

ToolbarAction DoggyToolbar::ActionFor(Hot hot) const {
  switch (hot) {
    case Hot::Rest:
      return ToolbarAction::ToggleRest;
    case Hot::Skin:
      return ToolbarAction::OpenSkinPicker;
    case Hot::Settings:
      return ToolbarAction::OpenSettings;
    case Hot::Mode:
    default:
      return ToolbarAction::ToggleMode;
  }
}

void DoggyToolbar::Render() {
  if (!hwnd_)
    return;
  EnsureGdiplus();
  const int width = window_rect_.right - window_rect_.left;
  const int height = window_rect_.bottom - window_rect_.top;
  if (width <= 0 || height <= 0)
    return;

  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  void* pixels = nullptr;
  HDC screen = GetDC(nullptr);
  HDC memory = CreateCompatibleDC(screen);
  HBITMAP bitmap =
      CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
  if (!bitmap || !memory) {
    if (bitmap)
      DeleteObject(bitmap);
    if (memory)
      DeleteDC(memory);
    if (screen)
      ReleaseDC(nullptr, screen);
    return;
  }
  const HGDIOBJ previous = SelectObject(memory, bitmap);

  {
    Gdiplus::Graphics graphics(memory);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    Gdiplus::SolidBrush transparent(Gdiplus::Color(0, 0, 0, 0));
    graphics.FillRectangle(&transparent, 0, 0, width, height);
  }

  {
    Gdiplus::Graphics graphics(memory);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

    // Plush capsule in the same warm palette as the candidate bar.
    const Gdiplus::RectF body(0.5f, 0.5f, static_cast<float>(width) - 1.0f,
                              static_cast<float>(height) - 1.0f);
    Gdiplus::GraphicsPath body_path;
    AddRoundRect(&body_path, body, body.Height * 0.5f);
    const bool highlighted = hot_ != Hot::None || state_ == State::Hover;
    Gdiplus::SolidBrush body_brush(highlighted ? Gdiplus::Color(253, 0xFF, 0xFC, 0xF6)
                                               : Plush(250));
    Gdiplus::Pen body_pen(Gdiplus::Color(120, 0xC8, 0xB2, 0x97), 1.0f);
    graphics.FillPath(&body_brush, &body_path);
    graphics.DrawPath(&body_pen, &body_path);

    // Mascot head, clipped to a circle.
    const RECT avatar = DipRect(kAvatarRect);
    if (const ThemeBitmap* head = assets_.Get(ThemeAssetId::MascotAvatar)) {
      Gdiplus::GraphicsPath clip;
      clip.AddEllipse(Gdiplus::RectF(
          static_cast<float>(avatar.left), static_cast<float>(avatar.top),
          static_cast<float>(avatar.right - avatar.left),
          static_cast<float>(avatar.bottom - avatar.top)));
      graphics.SetClip(&clip);
      DrawBitmap(memory, head, avatar, 255);
      graphics.ResetClip();
    }

    // Mode capsule plus glyph.
    const RECT mode = DipRect(kModeRect);
    Gdiplus::GraphicsPath mode_path;
    AddRoundRect(&mode_path,
                 Gdiplus::RectF(static_cast<float>(mode.left),
                                static_cast<float>(mode.top),
                                static_cast<float>(mode.right - mode.left),
                                static_cast<float>(mode.bottom - mode.top)),
                 static_cast<float>(mode.bottom - mode.top) * 0.5f);
    Gdiplus::SolidBrush mode_brush(hot_ == Hot::Mode ? Capsule(255)
                                                     : Capsule(235));
    graphics.FillPath(&mode_brush, &mode_path);
    if (const ThemeBitmap* glyph = assets_.Get(ascii_mode_
                                                   ? ThemeAssetId::ModeGlyphEn
                                                   : ThemeAssetId::ModeGlyphZh)) {
      RECT glyph_rect = mode;
      const int inset = (mode.bottom - mode.top) / 5;
      glyph_rect.left += inset;
      glyph_rect.top += inset;
      glyph_rect.right -= inset;
      glyph_rect.bottom -= inset;
      DrawBitmap(memory, glyph, glyph_rect, 255);
    }

    // Moon, shirt and gear in the brown ink colour.
    const ThemeAssetId icon_ids[] = {ThemeAssetId::TbIconMoon,
                                     ThemeAssetId::TbIconShirt,
                                     ThemeAssetId::TbIconGear};
    for (size_t index = 0; index < 3; ++index) {
      RECT slot = DipRect(kButtons[index + 1].rect);
      const int side = static_cast<int>(std::lround(18.0 * scale_));
      const int cx = (slot.left + slot.right) / 2;
      const int cy = (slot.top + slot.bottom) / 2;
      RECT icon = {cx - side / 2, cy - side / 2, cx + side / 2, cy + side / 2};
      const bool active = (index == 0 && hot_ == Hot::Rest) ||
                          (index == 1 && hot_ == Hot::Skin) ||
                          (index == 2 && hot_ == Hot::Settings);
      if (active) {
        Gdiplus::SolidBrush halo(Gdiplus::Color(90, 0xE8, 0xD9, 0xC9));
        Gdiplus::GraphicsPath halo_path;
        AddRoundRect(&halo_path,
                     Gdiplus::RectF(static_cast<float>(slot.left),
                                    static_cast<float>(slot.top),
                                    static_cast<float>(slot.right - slot.left),
                                    static_cast<float>(slot.bottom - slot.top)),
                     6.0f * scale_);
        graphics.FillPath(&halo, &halo_path);
      }
      DrawBitmap(memory, assets_.Get(icon_ids[index]), icon, 255);
    }

    graphics.Flush(Gdiplus::FlushIntentionSync);
  }

  POINT destination = {window_rect_.left, window_rect_.top};
  SIZE size = {width, height};
  POINT source = {0, 0};
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  UpdateLayeredWindow(hwnd_, screen, &destination, &size, memory, &source, 0,
                      &blend, ULW_ALPHA);

  SelectObject(memory, previous);
  DeleteObject(bitmap);
  DeleteDC(memory);
  if (screen)
    ReleaseDC(nullptr, screen);
}

LRESULT CALLBACK DoggyToolbar::WindowProc(HWND hwnd,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam) {
  if (message == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }
  auto* self =
      reinterpret_cast<DoggyToolbar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (self)
    return self->Handle(hwnd, message, wparam, lparam);
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT DoggyToolbar::Handle(HWND hwnd,
                             UINT message,
                             WPARAM wparam,
                             LPARAM lparam) {
  switch (message) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_MOUSEMOVE: {
      const int x = GET_X_LPARAM(lparam);
      const int y = GET_Y_LPARAM(lparam);
      if (!tracking_mouse_) {
        TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, hwnd_, 0};
        TrackMouseEvent(&track);
        tracking_mouse_ = true;
      }
      const Hot hot = HitTest(x, y);
      if (hot != hot_ || state_ != State::Hover) {
        hot_ = hot;
        state_ = State::Hover;
        Render();
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      tracking_mouse_ = false;
      hot_ = Hot::None;
      state_ = State::Default;
      Render();
      return 0;
    case WM_LBUTTONDOWN: {
      const int x = GET_X_LPARAM(lparam);
      const int y = GET_Y_LPARAM(lparam);
      const Hot hot = HitTest(x, y);
      if (hot != Hot::None) {
        pressed_ = hot;
        hot_ = hot;
        SetCapture(hwnd_);
        Render();
      }
      return 0;
    }
    case WM_LBUTTONUP: {
      const int x = GET_X_LPARAM(lparam);
      const int y = GET_Y_LPARAM(lparam);
      if (pressed_ != Hot::None) {
        const Hot released = pressed_;
        pressed_ = Hot::None;
        ReleaseCapture();
        const Hot hot = HitTest(x, y);
        hot_ = hot;
        Render();
        if (hot == released && handler_)
          handler_(ActionFor(hot));
      }
      return 0;
    }
    case WM_DESTROY:
      hwnd_ = nullptr;
      return 0;
    default:
      return DefWindowProcW(hwnd, message, wparam, lparam);
  }
}

}  // namespace leaves_stone
