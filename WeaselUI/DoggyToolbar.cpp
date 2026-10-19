#include "DoggyToolbar.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <string>

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
constexpr int kHeadLeft = 248, kHeadTop = 96;
constexpr int kHeadWidth = 400, kHeadHeight = 221;
constexpr int kDockDurationMs = 140;
constexpr int kSnapDistanceDip = 18;

void LogToolbar(const char* event, HWND window, ToolbarEdge edge) {
  static std::atomic<unsigned> count{0};
  if (count.fetch_add(1) >= 96) return;
  wchar_t temp[MAX_PATH]{};
  const DWORD length = GetTempPathW(MAX_PATH, temp);
  if (!length || length >= MAX_PATH) return;
  const std::wstring path = std::wstring(temp) + L"leaves-stone-toolbar.log";
  std::ofstream out(path.c_str(), std::ios::app | std::ios::binary);
  if (out) out << GetTickCount64() << " pid=" << GetCurrentProcessId()
               << " event=" << event << " visible=" << (IsWindowVisible(window) ? 1 : 0)
               << " edge=" << static_cast<int>(edge) << '\n';
}

RECT WorkAreaAt(POINT point) {
  MONITORINFO info{};
  info.cbSize = sizeof(info);
  const HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
  if (monitor && GetMonitorInfoW(monitor, &info)) return info.rcWork;
  RECT work{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  return work;
}

RECT OutsideEdge(RECT rect, const RECT& work, ToolbarEdge edge) {
  LONG dx = 0, dy = 0;
  if (edge == ToolbarEdge::Left) dx = work.left - rect.right - 1;
  if (edge == ToolbarEdge::Right) dx = work.right - rect.left + 1;
  if (edge == ToolbarEdge::Top) dy = work.top - rect.bottom - 1;
  if (edge == ToolbarEdge::Bottom) dy = work.bottom - rect.top + 1;
  OffsetRect(&rect, dx, dy);
  return rect;
}

RECT InterpolateRect(const RECT& from, const RECT& to, float progress) {
  const float rest = 1.0f - progress;
  const float eased = 1.0f - rest * rest * rest;
  const auto value = [eased](LONG a, LONG b) {
    return static_cast<LONG>(std::lround(a + (b - a) * eased));
  };
  return {value(from.left, to.left), value(from.top, to.top),
           value(from.right, to.right), value(from.bottom, to.bottom)};
}

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

bool DrawDoggyToolbarArtwork(HDC target, const ThemeAssets& assets,
                             bool awake, bool head_only, int width, int height) {
  if (!target || width <= 0 || height <= 0) return false;
  EnsureGdiplus();
  {
    Gdiplus::Graphics clear(target);
    clear.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    clear.Clear(Gdiplus::Color(0, 0, 0, 0));
  }
  const auto* frame = assets.Get(head_only
      ? (awake ? ThemeAssetId::RestHeadAwake : ThemeAssetId::RestHeadSleep)
      : (awake ? ThemeAssetId::RestAwake : ThemeAssetId::RestSleep));
  if (!frame) return false;
  if (!head_only) {
    const bool drawn = DrawBitmap(target, frame, 0, 0, width, height);
    const auto* decor = assets.Get(awake ? ThemeAssetId::RestDecorAwake
                                        : ThemeAssetId::RestDecorSleep);
    return DrawBitmap(target, decor, 0, 0, width, height) && drawn;
  }
  // Reuse the approved dog, with its existing alpha. Rotate its head 180
  // degrees at draw time; never regenerate or rotate the button labels.
  DIBSECTION dib{};
  if (!GetObjectW(frame->handle, sizeof(dib), &dib) || !dib.dsBm.bmBits)
    return false;
  if (frame->width < kHeadLeft + kHeadWidth || frame->height < kHeadTop + kHeadHeight)
    return false;
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = kHeadWidth;
  info.bmiHeader.biHeight = -kHeadHeight;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  void* rotated_pixels = nullptr;
  HBITMAP rotated = CreateDIBSection(target, &info, DIB_RGB_COLORS,
                                      &rotated_pixels, nullptr, 0);
  if (!rotated || !rotated_pixels) {
    if (rotated) DeleteObject(rotated);
    return false;
  }
  const auto* input = static_cast<const DWORD*>(dib.dsBm.bmBits);
  auto* output = static_cast<DWORD*>(rotated_pixels);
  const int stride = dib.dsBm.bmWidthBytes / 4;
  for (int y = 0; y < kHeadHeight; ++y)
    for (int x = 0; x < kHeadWidth; ++x)
      output[y * kHeadWidth + x] = input[
          (kHeadTop + kHeadHeight - 1 - y) * stride + kHeadLeft + kHeadWidth - 1 - x];
  const ThemeBitmap head{rotated, kHeadWidth, kHeadHeight};
  const bool drawn = DrawBitmap(target, &head, 0, 0, width, height);
  DeleteObject(rotated);
  return drawn;
}

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
  LogToolbar("created", hwnd_, dock_edge_);
  return true;
}

void DoggyToolbar::Destroy() {
  CancelPointer();
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
  LogToolbar(visible ? "show" : "hide", hwnd_, dock_edge_);
  if (!visible) {
    CancelPointer();
    CompleteDock();
    pointer_inside_ = false;
    hot_ = Hot::None;
    SetAwake(false);
  }
}

void DoggyToolbar::SetAsciiMode(bool) {
  // The supplied frames bake the mode cap, so there is nothing to relabel.
}

bool DoggyToolbar::HeadOnly() const {
  return dock_edge_ != ToolbarEdge::None && dock_stage_ != DockStage::Hide;
}

void DoggyToolbar::ReloadMetrics(UINT requested_dpi) {
  if (!requested_dpi && hwnd_) {
    using WindowDpi = UINT(WINAPI*)(HWND);
    static const auto window_dpi = reinterpret_cast<WindowDpi>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (window_dpi) requested_dpi = window_dpi(hwnd_);
  }
  HDC screen = GetDC(nullptr);
  const int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
  if (screen)
    ReleaseDC(nullptr, screen);
  const float dip_scale = static_cast<float>(requested_dpi ? requested_dpi : dpi) / 96.0f;
  if (requested_dpi || !hwnd_) scale_ = dip_scale * kSourceToDip * kUserScale;
  const int width = static_cast<int>(std::lround((HeadOnly() ? kHeadWidth : kCanvasPxWidth) * scale_));
  const int height = static_cast<int>(std::lround((HeadOnly() ? kHeadHeight : kCanvasPxHeight) * scale_));
  window_rect_.right = window_rect_.left + width;
  window_rect_.bottom = window_rect_.top + height;
}

void DoggyToolbar::ApplyPlacement() {
  RECT work = {0, 0, GetSystemMetrics(SM_CXSCREEN),
               GetSystemMetrics(SM_CYSCREEN)};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  work_area_ = work;
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

void DoggyToolbar::ApplyBounds() {
  SetWindowPos(hwnd_, HWND_TOPMOST, window_rect_.left, window_rect_.top,
                 window_rect_.right - window_rect_.left,
                 window_rect_.bottom - window_rect_.top, SWP_NOACTIVATE);
  Render();
}

RECT DoggyToolbar::ContentRect() const {
  if (HeadOnly())
    return {0, 0, window_rect_.right - window_rect_.left,
                   window_rect_.bottom - window_rect_.top};
  return SourceRectToWindow(kPanelLeft, kDogTop, kPanelWidth,
                             kPanelTop + kPanelHeight - kDogTop);
}

POINT DoggyToolbar::HeadCenter() const {
  if (HeadOnly()) return {(window_rect_.left + window_rect_.right) / 2,
                          (window_rect_.top + window_rect_.bottom) / 2};
  return {window_rect_.left + static_cast<LONG>(std::lround(448 * scale_)),
           window_rect_.top + static_cast<LONG>(std::lround(194 * scale_))};
}

void DoggyToolbar::ReflowForDisplay(UINT dpi) {
  if (button_down_ || dock_stage_ != DockStage::None) {
    metrics_pending_ = true;
    return;
  }
  metrics_pending_ = false;
  CompleteDock();
  const POINT center = HeadCenter();
  ReloadMetrics(dpi);
  work_area_ = WorkAreaAt(center);
  if (HeadOnly()) {
    const SIZE size{window_rect_.right - window_rect_.left,
                     window_rect_.bottom - window_rect_.top};
    window_rect_ = ToolbarPeekRect(center, size, work_area_, dock_edge_);
  } else {
    const POINT current_center = HeadCenter();
    OffsetRect(&window_rect_, center.x - current_center.x, center.y - current_center.y);
    window_rect_ = ClampToolbarWindow(window_rect_, ContentRect(), work_area_);
  }
  ApplyBounds();
}

void DoggyToolbar::CancelPointer() {
  button_down_ = false;
  dragging_ = false;
  pressed_ = Hot::None;
  if (hwnd_ && GetCapture() == hwnd_) ReleaseCapture();
  if (hwnd_ && metrics_pending_ && dock_stage_ == DockStage::None)
    ReflowForDisplay();
}

void DoggyToolbar::BeginPointer(int x, int y) {
  if (dock_stage_ != DockStage::None || !PointOnToolbar(x, y)) return;
  button_down_ = true;
  dragging_ = false;
  undocked_this_drag_ = false;
  pressed_ = HeadOnly() ? Hot::None : HitTest(x, y);
  press_point_ = {x, y};
  ClientToScreen(hwnd_, &press_point_);
  press_window_ = window_rect_;
  SetCapture(hwnd_);
}

void DoggyToolbar::MovePointer(int x, int y) {
  if (!button_down_) return;
  POINT cursor{x, y};
  ClientToScreen(hwnd_, &cursor);
  LONG dx = cursor.x - press_point_.x;
  LONG dy = cursor.y - press_point_.y;
  if (!dragging_ && std::abs(dx) < GetSystemMetrics(SM_CXDRAG) &&
      std::abs(dy) < GetSystemMetrics(SM_CYDRAG)) return;
  if (HeadOnly()) {
    const LONG inward = dock_edge_ == ToolbarEdge::Left ? dx :
                          dock_edge_ == ToolbarEdge::Right ? -dx :
                          dock_edge_ == ToolbarEdge::Top ? dy : -dy;
    const int pull_distance = static_cast<int>(std::lround(
        8.0f * scale_ / (kSourceToDip * kUserScale)));
    if (inward < pull_distance) {
      dragging_ = true;
      const SIZE head{press_window_.right - press_window_.left,
                       press_window_.bottom - press_window_.top};
      const POINT center{(press_window_.left + press_window_.right) / 2 + dx,
                           (press_window_.top + press_window_.bottom) / 2 + dy};
      window_rect_ = ToolbarPeekRect(center, head, work_area_, dock_edge_);
      ApplyBounds();
      return;
    }
    dock_edge_ = ToolbarEdge::None;
    undocked_this_drag_ = true;
    LogToolbar("pulled-out", hwnd_, dock_edge_);
    ReloadMetrics();
    const LONG width = window_rect_.right - window_rect_.left;
    const LONG height = window_rect_.bottom - window_rect_.top;
    window_rect_.left = cursor.x - static_cast<LONG>(std::lround(448 * scale_));
    window_rect_.top = cursor.y - static_cast<LONG>(std::lround(194 * scale_));
    window_rect_.right = window_rect_.left + width;
    window_rect_.bottom = window_rect_.top + height;
    press_window_ = window_rect_;
    press_point_ = cursor;
    dx = dy = 0;
  }
  if (!dragging_ || undocked_this_drag_) {
    dragging_ = true;
    pressed_ = Hot::None;
    SetAwake(true);
  }
  window_rect_ = press_window_;
  OffsetRect(&window_rect_, dx, dy);
  work_area_ = WorkAreaAt(cursor);
  window_rect_ = ClampToolbarWindow(window_rect_, ContentRect(), work_area_);
  ApplyBounds();
}

void DoggyToolbar::FinishPointer(int x, int y) {
  if (!button_down_) return;
  const bool dragged = dragging_;
  const bool pulled_out = undocked_this_drag_;
  const bool display_changed = metrics_pending_;
  const Hot pressed = pressed_;
  CancelPointer();
  if (dragged && !HeadOnly() && !pulled_out) {
    RECT content = ContentRect();
    OffsetRect(&content, window_rect_.left, window_rect_.top);
    const int threshold = static_cast<int>(std::lround(
        kSnapDistanceDip * scale_ / (kSourceToDip * kUserScale)));
    const auto edge = NearbyToolbarEdge(content, work_area_, threshold);
    if (edge != ToolbarEdge::None) Dock(edge);
  } else if (!dragged && !display_changed && !HeadOnly() && pressed != Hot::None && pressed == HitTest(x, y) && handler_) {
    handler_(ActionFor(pressed));
  }
}

void DoggyToolbar::Dock(ToolbarEdge edge) {
  const POINT center = HeadCenter();
  const SIZE head{static_cast<LONG>(std::lround(kHeadWidth * scale_)),
                   static_cast<LONG>(std::lround(kHeadHeight * scale_))};
  dock_final_ = ToolbarPeekRect(center, head, work_area_, edge);
  dock_edge_ = edge;
  LogToolbar("dock", hwnd_, dock_edge_);
  dock_stage_ = DockStage::Hide;
  dock_from_ = window_rect_;
  dock_to_ = OutsideEdge(window_rect_, work_area_, edge);
  dock_started_at_ = GetTickCount64();
}

void DoggyToolbar::AdvanceDock() {
  if (dock_stage_ == DockStage::None) return;
  const float progress = (std::min)(1.0f, static_cast<float>(
      GetTickCount64() - dock_started_at_) / kDockDurationMs);
  window_rect_ = InterpolateRect(dock_from_, dock_to_, progress);
  if (progress >= 1.0f) {
    if (dock_stage_ == DockStage::Hide) {
      dock_stage_ = DockStage::Peek;
      dock_from_ = OutsideEdge(dock_final_, work_area_, dock_edge_);
      dock_to_ = dock_final_;
      window_rect_ = dock_from_;
      dock_started_at_ = GetTickCount64();
    } else {
      dock_stage_ = DockStage::None;
      window_rect_ = dock_final_;
    }
  }
  ApplyBounds();
  if (dock_stage_ == DockStage::None && metrics_pending_) ReflowForDisplay();
}

void DoggyToolbar::CompleteDock() {
  if (dock_stage_ == DockStage::None) return;
  dock_stage_ = DockStage::None;
  window_rect_ = dock_final_;
  ApplyBounds();
  if (metrics_pending_) ReflowForDisplay();
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
  if (HeadOnly()) return x >= 0 && y >= 0 &&
      x < window_rect_.right - window_rect_.left &&
      y < window_rect_.bottom - window_rect_.top;
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
  if (HeadOnly()) return Hot::None;
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
  if (dock_stage_ != DockStage::None) { AdvanceDock(); return; }
  if (dragging_) return;
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

  DrawDoggyToolbarArtwork(memory, assets_, awake_, HeadOnly(), width, height);
  // Clip pixels to this monitor's work area. In particular, sliding toward the
  // bottom edge must not draw the disappearing bar over the Windows taskbar.
  auto* argb = static_cast<DWORD*>(pixels);
  const int visible_left = std::clamp<LONG>(work_area_.left - window_rect_.left, 0, width);
  const int visible_right = std::clamp<LONG>(work_area_.right - window_rect_.left, 0, width);
  for (int y = 0; y < height; ++y) {
    auto* row = argb + y * width;
    if (window_rect_.top + y < work_area_.top || window_rect_.top + y >= work_area_.bottom) {
      std::fill(row, row + width, 0);
    } else {
      std::fill(row, row + visible_left, 0);
      std::fill(row + visible_right, row + width, 0);
    }
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
    case WM_TIMER:
      if (wparam == kPollTimer) {
        PollPointer();
        return 0;
      }
      return 0;
    case WM_MOUSEMOVE: {
      MovePointer(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
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
    case WM_LBUTTONDOWN:
      BeginPointer(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
      return 0;
    case WM_LBUTTONUP: {
      FinishPointer(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
      return 0;
    }
    case WM_CANCELMODE:
    case WM_CAPTURECHANGED:
      CancelPointer();
      return 0;
    case WM_DPICHANGED:
      ReflowForDisplay(LOWORD(wparam));
      return 0;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
      ReflowForDisplay();
      return 0;
    case WM_NCHITTEST: {
      POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      ScreenToClient(hwnd_, &point);
      return PointOnToolbar(point.x, point.y) ? HTCLIENT : HTTRANSPARENT;
    }
    case WM_DESTROY:
      CancelPointer();
      hwnd_ = nullptr;
      return 0;
    default:
      return DefWindowProcW(hwnd, message, wparam, lparam);
  }
}

}  // namespace leaves_stone
