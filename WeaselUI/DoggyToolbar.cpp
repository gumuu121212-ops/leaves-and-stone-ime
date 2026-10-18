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

// Source canvas and anchors, straight from layout.json (896x480 at 2x).
constexpr int kCanvasPxWidth = 896;
constexpr int kCanvasPxHeight = 480;
constexpr float kSourceToDip = 0.5f;   // assetScale = 2
// The artwork targets a 448x240 dip canvas, twice the previous bar and oversized
// on a 1600x1000 desktop, so it is shown smaller than the spec size.
constexpr float kUserScale = 0.55f;
constexpr int kDogLeft = 248, kDogTop = 96, kDogWidth = 400, kDogHeight = 221;
constexpr int kPanelLeft = 128, kPanelTop = 288;
constexpr int kPanelWidth = 640, kPanelHeight = 160;
constexpr ULONGLONG kEnterDelayMs = 60;
constexpr ULONGLONG kLeaveDelayMs = 300;
constexpr UINT_PTR kPollTimer = 1;
constexpr int kPollMilliseconds = 30;
constexpr int kRightMarginDip = 28;
constexpr int kBottomMarginDip = 6;

struct ControlRect {
  RECT rect;
  ToolbarAction action;
};

constexpr ControlRect kControls[] = {
    {{246, 320, 342, 432}, ToolbarAction::ToggleMode},
    {{346, 320, 442, 432}, ToolbarAction::ToggleRest},
    {{450, 320, 546, 432}, ToolbarAction::OpenSkinPicker},
    {{554, 320, 650, 432}, ToolbarAction::OpenSettings}};

bool DrawBitmap(HDC target,
                const ThemeBitmap* bitmap,
                int x,
                int y,
                int width,
                int height) {
  if (!target || !bitmap || !bitmap->handle || width <= 0 || height <= 0)
    return false;
  HDC source = CreateCompatibleDC(target);
  if (!source)
    return false;
  const HGDIOBJ previous = SelectObject(source, bitmap->handle);
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  const BOOL ok = AlphaBlend(target, x, y, width, height, source, 0, 0,
                             bitmap->width, bitmap->height, blend);
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
  // Hidden until the input method is actually selected.
  SetTimer(hwnd_, kPollTimer, kPollMilliseconds, nullptr);
  return true;
}

void DoggyToolbar::Destroy() {
  if (hwnd_) {
    KillTimer(hwnd_, kPollTimer);
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  assets_.Discard();
}

void DoggyToolbar::SetVisible(bool visible) {
  if (!hwnd_)
    return;
  ShowWindow(hwnd_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
  if (!visible) {
    pointer_inside_ = false;
    hot_ = Hot::None;
    SetAwake(false);
  }
}

void DoggyToolbar::SetAsciiMode(bool) {
  // The supplied frames bake the mode cap, so there is nothing to relabel.
}

void DoggyToolbar::ReloadMetrics() {
  HDC screen = GetDC(nullptr);
  const int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
  if (screen)
    ReleaseDC(nullptr, screen);
  const float dip_scale = static_cast<float>(dpi) / 96.0f;
  scale_ = dip_scale * kSourceToDip * kUserScale;
  const int width = static_cast<int>(std::lround(kCanvasPxWidth * scale_));
  const int height = static_cast<int>(std::lround(kCanvasPxHeight * scale_));
  window_rect_ = {0, 0, width, height};
}

void DoggyToolbar::ApplyPlacement() {
  RECT work = {0, 0, GetSystemMetrics(SM_CXSCREEN),
               GetSystemMetrics(SM_CYSCREEN)};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  const float dip = scale_ / (kSourceToDip * kUserScale);
  const int width = window_rect_.right - window_rect_.left;
  const int height = window_rect_.bottom - window_rect_.top;
  // Align the whole window to the corner. The anchor sits inside the canvas
  // (448,296 of 896x480), so deriving the corner from it pushed the window off
  // the screen by the space to its right and below.
  const int x = work.right -
                static_cast<int>(std::lround(kRightMarginDip * dip)) - width;
  const int y = work.bottom -
                static_cast<int>(std::lround(kBottomMarginDip * dip)) - height;
  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height,
               SWP_NOACTIVATE);
  window_rect_ = {x, y, x + width, y + height};
}

RECT DoggyToolbar::SourceRectToWindow(int x, int y, int width, int height) const {
  RECT rect;
  rect.left = static_cast<LONG>(std::lround(x * scale_));
  rect.top = static_cast<LONG>(std::lround(y * scale_));
  rect.right = static_cast<LONG>(std::lround((x + width) * scale_));
  rect.bottom = static_cast<LONG>(std::lround((y + height) * scale_));
  return rect;
}

bool DoggyToolbar::PointOnToolbar(int x, int y) const {
  // Dog or visible bar; the transparent margin must pass clicks through.
  const RECT dog = SourceRectToWindow(kDogLeft, kDogTop, kDogWidth, kDogHeight);
  const RECT panel =
      SourceRectToWindow(kPanelLeft, kPanelTop, kPanelWidth, kPanelHeight);
  const bool inDog =
      x >= dog.left && x < dog.right && y >= dog.top && y < dog.bottom;
  const bool inPanel =
      x >= panel.left && x < panel.right && y >= panel.top && y < panel.bottom;
  return inDog || inPanel;
}

DoggyToolbar::Hot DoggyToolbar::HitTest(int x, int y) const {
  const struct {
    const ControlRect* control;
    Hot hot;
  } map[] = {{&kControls[0], Hot::Mode},
             {&kControls[1], Hot::Rest},
             {&kControls[2], Hot::Skin},
             {&kControls[3], Hot::Settings}};
  for (const auto& entry : map) {
    const RECT& r = entry.control->rect;
    const RECT rect =
        SourceRectToWindow(r.left, r.top, r.right - r.left, r.bottom - r.top);
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

void DoggyToolbar::SetAwake(bool awake) {
  if (awake_ == awake)
    return;
  awake_ = awake;
  Render();
}

void DoggyToolbar::PollPointer() {
  if (!hwnd_ || !IsWindowVisible(hwnd_))
    return;
  POINT cursor = {0, 0};
  if (!GetCursorPos(&cursor))
    return;
  ScreenToClient(hwnd_, &cursor);
  const bool inside = PointOnToolbar(cursor.x, cursor.y);
  const ULONGLONG now = GetTickCount64();
  if (inside != pointer_inside_) {
    pointer_inside_ = inside;
    pointer_changed_at_ = now;
  }
  if (inside) {
    if (!awake_ && now - pointer_changed_at_ >= kEnterDelayMs)
      SetAwake(true);
  } else if (awake_ && now - pointer_changed_at_ >= kLeaveDelayMs) {
    SetAwake(false);
  }
  const Hot hot = inside ? HitTest(cursor.x, cursor.y) : Hot::None;
  hot_ = hot;
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
    Gdiplus::Graphics clear(memory);
    clear.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    Gdiplus::SolidBrush transparent(Gdiplus::Color(0, 0, 0, 0));
    clear.FillRectangle(&transparent, 0, 0, width, height);
  }

  // Swap the whole frame; both share the same canvas and alpha.
  const ThemeAssetId frame =
      awake_ ? ThemeAssetId::RestAwake : ThemeAssetId::RestSleep;
  DrawBitmap(memory, assets_.Get(frame), 0, 0, width, height);
  // The little marks above the head live on their own layer: motion lines while
  // awake, drifting z's while asleep.
  const ThemeAssetId decoration =
      awake_ ? ThemeAssetId::RestDecorAwake : ThemeAssetId::RestDecorSleep;
  DrawBitmap(memory, assets_.Get(decoration), 0, 0, width, height);

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
    case WM_TIMER:
      if (wparam == kPollTimer) {
        PollPointer();
        return 0;
      }
      return 0;
    case WM_MOUSEMOVE: {
      if (!tracking_mouse_) {
        TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, hwnd_, 0};
        TrackMouseEvent(&track);
        tracking_mouse_ = true;
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      tracking_mouse_ = false;
      return 0;
    case WM_LBUTTONUP: {
      const int x = GET_X_LPARAM(lparam);
      const int y = GET_Y_LPARAM(lparam);
      const Hot hot = HitTest(x, y);
      if (hot != Hot::None && handler_)
        handler_(ActionFor(hot));
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
