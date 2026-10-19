#include <windows.h>
#include <cmath>
#include <iostream>

#include "include/ToolbarFocusState.h"
#include "WeaselUI/DoggyToolbar.h"
#include "WeaselTSF/DeferredToolbarActivation.h"

namespace {
int failures = 0;
void Check(bool ok, const char* message) {
  if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void Mouse(HWND window, UINT message, int x, int y) {
  SendMessageW(window, message,
               message == WM_LBUTTONUP ? 0 : MK_LBUTTON,
               MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y)));
}
void Pump(unsigned milliseconds) {
  const ULONGLONG end = GetTickCount64() + milliseconds;
  do {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message); DispatchMessageW(&message);
    }
    Sleep(1);
  } while (GetTickCount64() < end);
}
void TestDeferredActivationWithoutKeys() {
  leaves_stone::DeferredToolbarActivation activation;
  bool focus_ready = false;
  bool shown = false;
  const DWORD thread = GetCurrentThreadId();
  activation.Start([&]() {
    Check(GetCurrentThreadId() == thread,
          "activation reconciliation must stay on the original TSF thread");
    if (focus_ready) shown = true;
    return focus_ready;
  });
  Pump(20);
  Check(!shown, "an unsettled initial focus must not show the toolbar early");
  focus_ready = true;
  Pump(200);
  Check(shown, "deferred initial focus must show the toolbar without a key event");
  int stale = 0;
  activation.Start([&]() { ++stale; return false; });
  activation.Stop();
  Pump(180);
  Check(stale == 0, "deactivation must cancel pending focus work before it runs");
  activation.Start([&]() { ++stale; activation.Stop(); return false; });
  Pump(200);
  Check(stale == 1, "reentrant cancellation must not arm another activation timer");
}

void TestPlacementAcrossMonitors() {
  const RECT work{-1920, -1080, 0, 0};
  const RECT content{64, 48, 384, 224};
  auto clamped = leaves_stone::ClampToolbarWindow({-2200, -1300, -1752, -1060}, content, work);
  Check(clamped.left == -1984 && clamped.top == -1128,
        "clamping must use visible artwork and negative monitor coordinates");
  Check(leaves_stone::NearbyToolbarEdge({-1910, -900, -1600, -700}, work, 18) == leaves_stone::ToolbarEdge::Left,
        "the nearest screen edge must be detected on a negative-coordinate monitor");
  Check(leaves_stone::NearbyToolbarEdge({-1200, -800, -800, -500}, work, 18) == leaves_stone::ToolbarEdge::None,
        "a freely placed toolbar must not dock away from an edge");
  for (const float dpi : {1.0f, 1.25f, 1.5f, 2.0f}) {
    const SIZE head{static_cast<LONG>(160 * dpi), static_cast<LONG>(100 * dpi)};
    for (const auto edge : {leaves_stone::ToolbarEdge::Left, leaves_stone::ToolbarEdge::Top,
                            leaves_stone::ToolbarEdge::Right, leaves_stone::ToolbarEdge::Bottom}) {
      const auto rect = leaves_stone::ToolbarPeekRect({-900, -500}, head, work, edge);
      const LONG overlap_x = (std::min)(rect.right, work.right) - (std::max)(rect.left, work.left);
      const LONG overlap_y = (std::min)(rect.bottom, work.bottom) - (std::max)(rect.top, work.top);
      Check(overlap_x >= head.cx * 0.9f && overlap_y >= head.cy * 0.9f,
            "edge peeking must keep the face visible at every DPI");
      Check(rect.right - rect.left == head.cx && rect.bottom - rect.top == head.cy,
            "docking must preserve the original head proportions");
    }
  }
}

void TestDockAndPullOut() {
  int actions = 0;
  leaves_stone::DoggyToolbar toolbar(GetModuleHandleW(nullptr));
  if (!toolbar.Create([&](leaves_stone::ToolbarAction) { ++actions; })) {
    Check(false, "docking test must create the real toolbar"); return;
  }
  // Keep native test windows off the user's screen while exercising their real
  // visibility, sizing, capture and timer messages. No physical input is sent.
  SetWindowRgn(toolbar.hwnd(), CreateRectRgn(0, 0, 0, 0), FALSE);
  toolbar.SetVisible(true);
  RECT before{}; GetWindowRect(toolbar.hwnd(), &before);
  const float scale = (before.right - before.left) / 896.0f;
  const int x = static_cast<int>(std::lround(448 * scale));
  const int y = static_cast<int>(std::lround(194 * scale));
  RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  const int target_x = work.left - static_cast<int>(std::lround(128 * scale));
  const int target_y = work.top + 200 - static_cast<int>(std::lround(96 * scale));
  Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, x, y);
  Mouse(toolbar.hwnd(), WM_MOUSEMOVE, x + target_x - before.left, y + target_y - before.top);
  Mouse(toolbar.hwnd(), WM_LBUTTONUP, x, y);
  Pump(420);
  RECT head{}; GetWindowRect(toolbar.hwnd(), &head);
  Check(head.right - head.left < (before.right - before.left) / 2,
        "releasing at the monitor edge must hide the capsule and leave only the head");
  const int hx = (head.right - head.left) * 3 / 4;
  const int hy = (head.bottom - head.top) / 2;
  Mouse(toolbar.hwnd(), WM_MOUSEMOVE, hx, hy);
  Pump(100);
  RECT hovered{}; GetWindowRect(toolbar.hwnd(), &hovered);
  Check(EqualRect(&head, &hovered), "hovering the docked head must not expand the toolbar");
  Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, hx, hy);
  Mouse(toolbar.hwnd(), WM_MOUSEMOVE, hx, hy + 25);
  Mouse(toolbar.hwnd(), WM_LBUTTONUP, hx, hy);
  RECT slid{}; GetWindowRect(toolbar.hwnd(), &slid);
  Check(slid.top == head.top + 25 && slid.right - slid.left == head.right - head.left,
        "dragging along an edge must move the head without expanding it");
  Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, hx, hy);
  Mouse(toolbar.hwnd(), WM_MOUSEMOVE, hx + 70, hy);
  Mouse(toolbar.hwnd(), WM_LBUTTONUP, x, y);
  Pump(350);
  RECT restored{}; GetWindowRect(toolbar.hwnd(), &restored);
  Check(restored.right - restored.left == before.right - before.left &&
            restored.bottom - restored.top == before.bottom - before.top,
        "pulling the head away must restore the full toolbar without instantly redocking");
  Check(actions == 0, "docking and pulling out must never click a toolbar control");
  toolbar.SetVisible(false);
  SendMessageW(toolbar.hwnd(), WM_DISPLAYCHANGE, 0, 0);
  SendMessageW(toolbar.hwnd(), WM_TIMER, 1, 0);
  Check(!IsWindowVisible(toolbar.hwnd()), "display changes must not show an inactive toolbar");
}

void TestOtherDockEdgesAndCancellation() {
  for (const auto edge : {leaves_stone::ToolbarEdge::Top,
                          leaves_stone::ToolbarEdge::Right,
                          leaves_stone::ToolbarEdge::Bottom}) {
    leaves_stone::DoggyToolbar toolbar(GetModuleHandleW(nullptr));
    if (!toolbar.Create([](leaves_stone::ToolbarAction) {})) {
      Check(false, "other-edge test must create the real toolbar"); return;
    }
    SetWindowRgn(toolbar.hwnd(), CreateRectRgn(0, 0, 0, 0), FALSE);
    toolbar.SetVisible(true);
    RECT before{}; GetWindowRect(toolbar.hwnd(), &before);
    const float scale = (before.right - before.left) / 896.0f;
    const int x = static_cast<int>(std::lround(448 * scale));
    const int y = static_cast<int>(std::lround(194 * scale));
    RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int left = work.left + 250;
    int top = work.top + 180;
    if (edge == leaves_stone::ToolbarEdge::Top)
      top = work.top - static_cast<int>(std::lround(96 * scale));
    if (edge == leaves_stone::ToolbarEdge::Right)
      left = work.right - static_cast<int>(std::lround(768 * scale));
    if (edge == leaves_stone::ToolbarEdge::Bottom)
      top = work.bottom - static_cast<int>(std::lround(448 * scale));
    Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, x, y);
    Mouse(toolbar.hwnd(), WM_MOUSEMOVE, x + left - before.left, y + top - before.top);
    Mouse(toolbar.hwnd(), WM_LBUTTONUP, x, y);
    Pump(390);
    RECT head{}; GetWindowRect(toolbar.hwnd(), &head);
    Check(head.right - head.left < (before.right - before.left) / 2,
          "top, right and bottom docking must also leave a head-only window");
    const int hx = (head.right - head.left) / 2;
    const int hy = (head.bottom - head.top) / 2;
    const int dx = edge == leaves_stone::ToolbarEdge::Right ? -70 : 0;
    const int dy = edge == leaves_stone::ToolbarEdge::Top ? 70 :
                     edge == leaves_stone::ToolbarEdge::Bottom ? -70 : 0;
    Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, hx, hy);
    Mouse(toolbar.hwnd(), WM_MOUSEMOVE, hx + dx, hy + dy);
    Mouse(toolbar.hwnd(), WM_LBUTTONUP, x, y);
    RECT restored{}; GetWindowRect(toolbar.hwnd(), &restored);
    Check(restored.right - restored.left == before.right - before.left,
          "pulling inward from each edge must restore the full toolbar");
    Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, x, y);
    SendMessageW(toolbar.hwnd(), WM_DPICHANGED, MAKELONG(144, 144), 0);
    SendMessageW(toolbar.hwnd(), WM_CANCELMODE, 0, 0);
    Check(GetCapture() != toolbar.hwnd(),
          "cancelling a drag during a DPI change must release capture safely");
    toolbar.SetVisible(false);
  }
}
void TestFocusBeforeToolbarCreation() {
  leaves_stone::ToolbarFocusState focus;
  focus.FocusIn(42);
  leaves_stone::DoggyToolbar toolbar(GetModuleHandleW(nullptr));
  if (!toolbar.Create([](leaves_stone::ToolbarAction) {})) {
    Check(false, "late-focus test must create the real toolbar"); return;
  }
  focus.Bind([&](bool active) { toolbar.SetVisible(active); });
  Check(IsWindowVisible(toolbar.hwnd()),
        "binding a newly created toolbar must replay focus without typing");
  focus.FocusIn(84);
  focus.FocusOut(42);
  Check(IsWindowVisible(toolbar.hwnd()),
        "a late background FocusOut must not hide the active toolbar");
  focus.FocusOut(84);
  Check(!IsWindowVisible(toolbar.hwnd()),
        "the current FocusOut must hide the toolbar");
  focus.FocusIn(42);
  focus.Reset();
  focus.Bind([&](bool active) { toolbar.SetVisible(active); });
  Check(!IsWindowVisible(toolbar.hwnd()),
        "rebinding after deactivation must not resurrect the toolbar");
}
void TestDragAndButtons() {
  int actions = 0;
  leaves_stone::DoggyToolbar toolbar(GetModuleHandleW(nullptr));
  if (!toolbar.Create([&](leaves_stone::ToolbarAction) { ++actions; })) {
    Check(false, "drag test must create the real toolbar"); return;
  }
  RECT before{}; GetWindowRect(toolbar.hwnd(), &before);
  const float scale = (before.right - before.left) / 896.0f;
  const int x = static_cast<int>(std::lround(448 * scale));
  const int y = static_cast<int>(std::lround(190 * scale));
  Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, x, y);
  Mouse(toolbar.hwnd(), WM_MOUSEMOVE, x - 80, y - 45);
  Mouse(toolbar.hwnd(), WM_LBUTTONUP, x, y);
  RECT after{}; GetWindowRect(toolbar.hwnd(), &after);
  Check(after.left == before.left - 80 && after.top == before.top - 45,
        "dragging the dog must move the whole toolbar by the pointer delta");
  Check(actions == 0, "dragging the dog must not activate a toolbar button");
  const int button_x = static_cast<int>(std::lround(294 * scale));
  const int button_y = static_cast<int>(std::lround(376 * scale));
  Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, button_x, button_y);
  Mouse(toolbar.hwnd(), WM_LBUTTONUP, button_x, button_y);
  Check(actions == 1, "a normal button click must still run exactly one action");
  Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, button_x, button_y);
  Mouse(toolbar.hwnd(), WM_MOUSEMOVE, button_x - 12, button_y - 12);
  Mouse(toolbar.hwnd(), WM_LBUTTONUP, button_x, button_y);
  Check(actions == 1, "moving a pressed button into a drag must suppress its click");
  Check(GetCapture() != toolbar.hwnd(), "releasing a drag must release mouse capture");
  toolbar.SetVisible(false);
  SendMessageW(toolbar.hwnd(), WM_TIMER, 1, 0);
  Check(!IsWindowVisible(toolbar.hwnd()), "a hidden dragged toolbar must stay hidden");
}

void TestReleaseOutsideBackgroundToolbar() {
  const int primary_button = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
  // Do not interfere with a physical drag the user might be performing.
  if (GetAsyncKeyState(primary_button) & 0x8000) return;
  leaves_stone::DoggyToolbar toolbar(GetModuleHandleW(nullptr));
  if (!toolbar.Create([](leaves_stone::ToolbarAction) {})) {
    Check(false, "background release test must create the real toolbar"); return;
  }
  SetWindowRgn(toolbar.hwnd(), CreateRectRgn(0, 0, 0, 0), FALSE);
  toolbar.SetVisible(true);
  RECT bounds{}; GetWindowRect(toolbar.hwnd(), &bounds);
  const float scale = (bounds.right - bounds.left) / 896.0f;
  Mouse(toolbar.hwnd(), WM_LBUTTONDOWN,
        static_cast<int>(std::lround(448 * scale)),
        static_cast<int>(std::lround(194 * scale)));
  SendMessageW(toolbar.hwnd(), WM_TIMER, 1, 0);
  Check(GetCapture() != toolbar.hwnd(),
        "pointer polling must finish a background drag when the release message was lost");
  toolbar.SetVisible(false);
}
}  // namespace

int RunToolbarInteractionTests() {
  TestDeferredActivationWithoutKeys();
  TestPlacementAcrossMonitors();
  TestFocusBeforeToolbarCreation();
  TestDragAndButtons();
  TestReleaseOutsideBackgroundToolbar();
  TestDockAndPullOut();
  TestOtherDockEdgesAndCancellation();
  return failures;
}
