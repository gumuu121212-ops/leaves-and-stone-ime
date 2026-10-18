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
constexpr int kCanvasNativeWidth = 896;
constexpr int kCanvasNativeHeight = 480;
constexpr int kCanvasDipWidth = 224;
constexpr int kCanvasDipHeight = 120;
constexpr int kIdleMilliseconds = 12000;
constexpr int kHoverDelayMilliseconds = 100;
constexpr UINT_PTR kHoverTimer = 1;
constexpr UINT_PTR kIdleTimer = 2;
constexpr int kCornerMarginDip = 24;

// Native source rectangles, straight from layout.json.
struct ControlRects {
  RECT hit;
  RECT image;
};

constexpr ControlRects kModeRects = {{416, 304, 516, 432}, {434, 336, 498, 400}};
constexpr ControlRects kRestRects = {{520, 304, 620, 432}, {534, 332, 606, 404}};
constexpr ControlRects kSkinRects = {{624, 304, 724, 432}, {638, 332, 710, 404}};
constexpr ControlRects kSettingsRects = {{728, 304, 828, 432},
                                         {742, 332, 814, 404}};
constexpr RECT kModeGlyph = {442, 344, 490, 392};

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

bool PointInRect(const RECT& rect, int x, int y) {
  return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
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
  SetTimer(hwnd_, kIdleTimer, kIdleMilliseconds, nullptr);
  return true;
}

void DoggyToolbar::Destroy() {
  if (hwnd_) {
    KillTimer(hwnd_, kHoverTimer);
    KillTimer(hwnd_, kIdleTimer);
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
  // The artwork is authored at 4x, so one source pixel is a quarter of a dip.
  scale_ = static_cast<float>(dpi) / 96.0f / 4.0f;
  const int width = static_cast<int>(
      std::lround(kCanvasDipWidth * static_cast<float>(dpi) / 96.0f));
  const int height = static_cast<int>(
      std::lround(kCanvasDipHeight * static_cast<float>(dpi) / 96.0f));
  window_rect_ = {0, 0, width, height};
}

void DoggyToolbar::ApplyPlacement() {
  RECT work = {0, 0, GetSystemMetrics(SM_CXSCREEN),
               GetSystemMetrics(SM_CYSCREEN)};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  const int margin = static_cast<int>(
      std::lround(kCornerMarginDip * scale_ * 4.0f));
  const int width = window_rect_.right - window_rect_.left;
  const int height = window_rect_.bottom - window_rect_.top;
  const int x = work.right - width - margin;
  const int y = work.bottom - height - margin;
  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  window_rect_ = {x, y, x + width, y + height};
}

RECT DoggyToolbar::NativeRectToWindow(int x,
                                      int y,
                                      int width,
                                      int height) const {
  RECT rect;
  rect.left = static_cast<LONG>(std::lround(x * scale_));
  rect.top = static_cast<LONG>(std::lround(y * scale_));
  rect.right = static_cast<LONG>(std::lround((x + width) * scale_));
  rect.bottom = static_cast<LONG>(std::lround((y + height) * scale_));
  return rect;
}

bool DoggyToolbar::PointInVisibleArea(int x, int y) const {
  // Base panel plus the mascot, matching the silhouette of the current state.
  const RECT panel =
      (state_ == State::Idle) ? RECT{96, 320, 832, 448} : RECT{192, 288, 832, 448};
  const RECT dog = (state_ == State::Idle)    ? RECT{96, 148, 616, 436}
                   : (state_ == State::Default) ? RECT{72, 204, 352, 428}
                                                : RECT{12, 116, 412, 337};
  return PointInRect(NativeRectToWindow(panel.left, panel.top,
                                        panel.right - panel.left,
                                        panel.bottom - panel.top),
                     x, y) ||
         PointInRect(NativeRectToWindow(dog.left, dog.top, dog.right - dog.left,
                                        dog.bottom - dog.top),
                     x, y);
}

DoggyToolbar::Hot DoggyToolbar::HitTest(int x, int y) const {
  if (state_ == State::Idle)
    return Hot::None;
  const struct {
    const ControlRects* rects;
    Hot hot;
  } targets[] = {{&kModeRects, Hot::Mode},
                 {&kRestRects, Hot::Rest},
                 {&kSkinRects, Hot::Skin},
                 {&kSettingsRects, Hot::Settings}};
  for (const auto& target : targets) {
    const RECT hit = NativeRectToWindow(
        target.rects->hit.left, target.rects->hit.top,
        target.rects->hit.right - target.rects->hit.left,
        target.rects->hit.bottom - target.rects->hit.top);
    if (PointInRect(hit, x, y))
      return target.hot;
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

void DoggyToolbar::SetState(State state) {
  if (state_ == state)
    return;
  state_ = state;
  Render();
}

void DoggyToolbar::Wake() {
  KillTimer(hwnd_, kIdleTimer);
  SetTimer(hwnd_, kIdleTimer, kIdleMilliseconds, nullptr);
  if (state_ == State::Idle)
    SetState(State::Default);
}

void DoggyToolbar::Render() {
  if (!hwnd_)
    return;
  const int width = window_rect_.right - window_rect_.left;
  const int height = window_rect_.bottom - window_rect_.top;
  if (width <= 0 || height <= 0)
    return;
  EnsureGdiplus();


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
  if (!bitmap || !memory || !pixels) {
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

  ThemeAssetId base = ThemeAssetId::ToolbarDefault;
  switch (state_) {
    case State::Hover:
      base = ThemeAssetId::ToolbarHover;
      break;
    case State::Pressed:
      base = ThemeAssetId::ToolbarPressed;
      break;
    case State::Idle:
      base = ThemeAssetId::ToolbarIdle;
      break;
    default:
      break;
  }
  const RECT canvas = {0, 0, width, height};
  if (const ThemeBitmap* layer = assets_.Get(base))
    DrawBitmap(memory, layer, canvas, 255);

  if (state_ != State::Idle) {
    // Button feedback sits between the base layer and the icons.
    if (hot_ != Hot::None) {
      const ControlRects* rects = nullptr;
      switch (hot_) {
        case Hot::Mode:
          rects = &kModeRects;
          break;
        case Hot::Rest:
          rects = &kRestRects;
          break;
        case Hot::Skin:
          rects = &kSkinRects;
          break;
        case Hot::Settings:
          rects = &kSettingsRects;
          break;
        default:
          break;
      }
      if (rects) {
        const ThemeAssetId id = (state_ == State::Pressed && pressed_ == hot_)
                                    ? ThemeAssetId::ButtonPressed
                                    : ThemeAssetId::ButtonHover;
        if (const ThemeBitmap* feedback = assets_.Get(id)) {
          DrawBitmap(memory, feedback,
                     NativeRectToWindow(rects->hit.left, rects->hit.top,
                                        rects->hit.right - rects->hit.left,
                                        rects->hit.bottom - rects->hit.top),
                     255);
        }
      }
    }

    if (const ThemeBitmap* background = assets_.Get(ThemeAssetId::ModeBackground)) {
      DrawBitmap(memory, background,
                 NativeRectToWindow(kModeRects.image.left, kModeRects.image.top,
                                    kModeRects.image.right -
                                        kModeRects.image.left,
                                    kModeRects.image.bottom -
                                        kModeRects.image.top),
                 255);
    }
    if (const ThemeBitmap* glyph = assets_.Get(ascii_mode_
                                                   ? ThemeAssetId::ModeGlyphEn
                                                   : ThemeAssetId::ModeGlyphZh)) {
      DrawBitmap(memory, glyph,
                 NativeRectToWindow(kModeGlyph.left, kModeGlyph.top,
                                    kModeGlyph.right - kModeGlyph.left,
                                    kModeGlyph.bottom - kModeGlyph.top),
                 255);
    }
    const struct {
      const ControlRects* rects;
      ThemeAssetId id;
    } icons[] = {{&kRestRects, ThemeAssetId::TbIconMoon},
                 {&kSkinRects, ThemeAssetId::TbIconShirt},
                 {&kSettingsRects, ThemeAssetId::TbIconGear}};
    for (const auto& icon : icons) {
      if (const ThemeBitmap* image = assets_.Get(icon.id)) {
        DrawBitmap(memory, image,
                   NativeRectToWindow(
                       icon.rects->image.left, icon.rects->image.top,
                       icon.rects->image.right - icon.rects->image.left,
                       icon.rects->image.bottom - icon.rects->image.top),
                   255);
      }
    }

    ThemeAssetId decoration = ThemeAssetId::Count;
    if (state_ == State::Pressed)
      decoration = ThemeAssetId::DecorPressed;
    else if (state_ == State::Hover)
      decoration = ThemeAssetId::DecorHover;
    if (decoration != ThemeAssetId::Count) {
      if (const ThemeBitmap* image = assets_.Get(decoration))
        DrawBitmap(memory, image, canvas, 255);
    }
  } else if (const ThemeBitmap* decoration = assets_.Get(ThemeAssetId::DecorIdle)) {
    DrawBitmap(memory, decoration, canvas, 255);
  }

  // UpdateLayeredWindow expects premultiplied alpha; the drawing above produced
  // straight alpha.
  const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
  auto* argb = static_cast<DWORD*>(pixels);
  for (size_t index = 0; index < count; ++index) {
    const DWORD value = argb[index];
    const BYTE alpha = static_cast<BYTE>(value >> 24);
    if (alpha == 0 || alpha == 255)
      continue;
    const BYTE blue = static_cast<BYTE>((value >> 0) * alpha / 255);
    const BYTE green = static_cast<BYTE>((value >> 8) * alpha / 255);
    const BYTE red = static_cast<BYTE>((value >> 16) * alpha / 255);
    argb[index] = (static_cast<DWORD>(alpha) << 24) |
                  (static_cast<DWORD>(red) << 16) |
                  (static_cast<DWORD>(green) << 8) | static_cast<DWORD>(blue);
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
      if (dragging_) {
        POINT cursor;
        GetCursorPos(&cursor);
        SetWindowPos(hwnd_, HWND_TOPMOST,
                     drag_window_.left + cursor.x - drag_origin_.x,
                     drag_window_.top + cursor.y - drag_origin_.y, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE);
        GetWindowRect(hwnd_, &window_rect_);
        return 0;
      }
      if (!tracking_mouse_) {
        TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, hwnd_, 0};
        TrackMouseEvent(&track);
        tracking_mouse_ = true;
      }
      Wake();
      const Hot hot = HitTest(x, y);
      if (hot != hot_) {
        hot_ = hot;
        Render();
      }
      if (!hover_pending_ && state_ != State::Pressed &&
          PointInVisibleArea(x, y)) {
        hover_pending_ = true;
        SetTimer(hwnd_, kHoverTimer, kHoverDelayMilliseconds, nullptr);
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      tracking_mouse_ = false;
      hot_ = Hot::None;
      if (state_ != State::Pressed)
        SetState(State::Default);
      else
        Render();
      return 0;
    case WM_TIMER:
      if (wparam == kHoverTimer) {
        KillTimer(hwnd_, kHoverTimer);
        hover_pending_ = false;
        if (state_ != State::Pressed)
          SetState(State::Hover);
        return 0;
      }
      if (wparam == kIdleTimer) {
        KillTimer(hwnd_, kIdleTimer);
        if (state_ != State::Pressed && hot_ == Hot::None)
          SetState(State::Idle);
        return 0;
      }
      return 0;
    case WM_LBUTTONDOWN: {
      const int x = GET_X_LPARAM(lparam);
      const int y = GET_Y_LPARAM(lparam);
      Wake();
      if (state_ == State::Idle) {
        // Waking up must not trigger a button behind the sleeping pet.
        SetState(State::Default);
        return 0;
      }
      const Hot hot = HitTest(x, y);
      if (hot != Hot::None) {
        pressed_ = hot;
        SetState(State::Pressed);
        SetCapture(hwnd_);
        return 0;
      }
      if (PointInVisibleArea(x, y)) {
        dragging_ = true;
        GetCursorPos(&drag_origin_);
        GetWindowRect(hwnd_, &drag_window_);
        SetCapture(hwnd_);
      }
      return 0;
    }
    case WM_LBUTTONUP: {
      const int x = GET_X_LPARAM(lparam);
      const int y = GET_Y_LPARAM(lparam);
      if (dragging_) {
        dragging_ = false;
        ReleaseCapture();
        return 0;
      }
      if (pressed_ != Hot::None) {
        const Hot hot = HitTest(x, y);
        const Hot released = pressed_;
        pressed_ = Hot::None;
        ReleaseCapture();
        SetState(hot == Hot::None ? State::Default : State::Hover);
        if (hot == released && handler_)
          handler_(ActionFor(hot));
        return 0;
      }
      return 0;
    }
    case WM_RBUTTONUP: {
      DestroyWindow(hwnd_);
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
