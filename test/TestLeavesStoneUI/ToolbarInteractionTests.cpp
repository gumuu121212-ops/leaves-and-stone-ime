#include <windows.h>
#include <cmath>
#include <iostream>
#include <string>

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
               message == WM_RBUTTONDOWN ? MK_RBUTTON :
                   (message == WM_LBUTTONUP || message == WM_RBUTTONUP ? 0 : MK_LBUTTON),
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

// Drive only this test window's modal menu queue; never inject physical input
// or operate on another application's windows.
struct ToolbarMenuProbe {
  HWND owner;
  WNDPROC previous = nullptr;
  UINT_PTR timer = 0;
  bool choose_hide;
  bool opened = false;
  bool visible_while_open = false;
  bool has_hide_item = false;
  bool interaction_sent = false;
  bool timed_out = false;
  unsigned ticks = 0;
  static ToolbarMenuProbe* current;

  static LRESULT CALLBACK Procedure(HWND window, UINT message,
                                     WPARAM wparam, LPARAM lparam) {
    auto& probe = *current;
    if (message == WM_INITMENUPOPUP) {
      probe.opened = true;
      probe.visible_while_open = IsWindowVisible(window) != FALSE;
      wchar_t label[32]{};
      const auto menu = reinterpret_cast<HMENU>(wparam);
      GetMenuStringW(menu, 0, label, 32, MF_BYPOSITION);
      probe.has_hide_item = GetMenuItemCount(menu) == 1 &&
                            std::wstring(label) == L"\u9690\u85cf";
    }
    return CallWindowProcW(probe.previous, window, message, wparam, lparam);
  }
  static void CALLBACK OnTimer(HWND, UINT, UINT_PTR timer_id, DWORD) {
    if (!current || current->timer != timer_id) return;
    auto& probe = *current;
    if (++probe.ticks > 30) {
      probe.timed_out = true;
      EndMenu();
      return;
    }
    if (!probe.opened || probe.ticks != 2) return;
    probe.interaction_sent = true;
    if (probe.choose_hide) {
      PostMessageW(probe.owner, WM_KEYDOWN, VK_DOWN, 0);
      PostMessageW(probe.owner, WM_KEYDOWN, VK_RETURN, 0);
    } else {
      PostMessageW(probe.owner, WM_KEYDOWN, VK_ESCAPE, 0);
    }
  }
  ToolbarMenuProbe(HWND window, bool hide) : owner(window), choose_hide(hide) {
    current = this;
    previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        owner, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Procedure)));
    timer = SetTimer(nullptr, 0, 10, OnTimer);
  }
  ~ToolbarMenuProbe() {
    KillTimer(nullptr, timer);
    SetWindowLongPtrW(owner, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
    current = nullptr;
  }
};
ToolbarMenuProbe* ToolbarMenuProbe::current = nullptr;

void RightClickMenu(HWND window, int x, int y, bool hide) {
  ToolbarMenuProbe probe(window, hide);
  Mouse(window, WM_RBUTTONDOWN, x, y);
  Mouse(window, WM_RBUTTONUP, x, y);
  Check(probe.opened && probe.has_hide_item,
        "right-click must open a menu containing the Hide item");
  Check(probe.visible_while_open,
        "opening the right-click menu must leave the toolbar visible");
  Check(probe.interaction_sent && !probe.timed_out,
        "the menu must exit via the requested selection or Escape, not timeout");
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
void TestDragKeepsFullToolbarAtEveryEdge() {
  for (int edge = 0; edge < 4; ++edge) {
    leaves_stone::DoggyToolbar toolbar(GetModuleHandleW(nullptr));
    if (!toolbar.Create([](leaves_stone::ToolbarAction) {})) {
      Check(false, "edge drag test must create the real toolbar"); return;
    }
    SetWindowRgn(toolbar.hwnd(), CreateRectRgn(0, 0, 0, 0), FALSE);
    toolbar.SetVisible(true);
    RECT before{}; GetWindowRect(toolbar.hwnd(), &before);
    const float scale = (before.right - before.left) / 896.0f;
    const int x = static_cast<int>(std::lround(448 * scale));
    const int y = static_cast<int>(std::lround(194 * scale));
    RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int left = work.left + 250, top = work.top + 180;
    if (edge == 0) left = work.left - static_cast<int>(std::lround(128 * scale));
    if (edge == 1) top = work.top - static_cast<int>(std::lround(96 * scale));
    if (edge == 2) left = work.right - static_cast<int>(std::lround(768 * scale));
    if (edge == 3) top = work.bottom - static_cast<int>(std::lround(448 * scale));
    Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, x, y);
    Mouse(toolbar.hwnd(), WM_MOUSEMOVE, x + left - before.left, y + top - before.top);
    Mouse(toolbar.hwnd(), WM_LBUTTONUP, x, y);
    Pump(400);
    RECT after{}; GetWindowRect(toolbar.hwnd(), &after);
    Check(after.right - after.left == before.right - before.left &&
              after.bottom - after.top == before.bottom - before.top && IsWindowVisible(toolbar.hwnd()),
          "releasing at any screen edge must leave the complete toolbar visible");
    toolbar.SetVisible(false);
  }
}

void TestRightClickHideSurvivesFocusChanges() {
  int actions = 0;
  leaves_stone::DoggyToolbar toolbar(GetModuleHandleW(nullptr));
  if (!toolbar.Create([&](leaves_stone::ToolbarAction) { ++actions; })) {
    Check(false, "right-click test must create the real toolbar"); return;
  }
  SetWindowRgn(toolbar.hwnd(), CreateRectRgn(0, 0, 0, 0), FALSE);
  toolbar.SetVisible(true);
  RECT bounds{}; GetWindowRect(toolbar.hwnd(), &bounds);
  const float scale = (bounds.right - bounds.left) / 896.0f;
  const int x = static_cast<int>(std::lround(448 * scale));
  const int y = static_cast<int>(std::lround(194 * scale));
  Mouse(toolbar.hwnd(), WM_RBUTTONUP, 0, 0);
  Check(IsWindowVisible(toolbar.hwnd()), "right-clicking transparent margins must not hide the toolbar");
  Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, x, y);
  Mouse(toolbar.hwnd(), WM_MOUSEMOVE, x - 70, y - 40);
  Mouse(toolbar.hwnd(), WM_LBUTTONUP, x, y);
  GetWindowRect(toolbar.hwnd(), &bounds);
  const HWND foreground = GetForegroundWindow();
  RightClickMenu(toolbar.hwnd(), x, y, false);
  Check(IsWindowVisible(toolbar.hwnd()),
        "dismissing the context menu must not hide the toolbar");
  RightClickMenu(toolbar.hwnd(), x, y, true);
  Check(!IsWindowVisible(toolbar.hwnd()),
        "choosing Hide from the context menu must hide the toolbar");
  Check(GetForegroundWindow() == foreground && actions == 0,
        "right-click hide must not activate the toolbar or click a button");
  toolbar.SetVisible(true);
  Check(!IsWindowVisible(toolbar.hwnd()), "typing focus notifications must respect manual hiding");
  toolbar.SetVisible(false);
  toolbar.SetVisible(true);
  SendMessageW(toolbar.hwnd(), WM_DISPLAYCHANGE, 0, 0);
  SendMessageW(toolbar.hwnd(), WM_TIMER, 1, 0);
  Check(!IsWindowVisible(toolbar.hwnd()), "focus and display changes must not undo manual hiding");
  toolbar.SetUserHidden(false);
  Check(IsWindowVisible(toolbar.hwnd()), "the tray show action must restore a manually hidden toolbar");
  RECT restored{}; GetWindowRect(toolbar.hwnd(), &restored);
  Check(EqualRect(&bounds, &restored), "hiding and restoring must preserve the dragged toolbar position");
  toolbar.SetVisible(false);
  toolbar.SetUserHidden(true);
  toolbar.SetUserHidden(false);
  Check(!IsWindowVisible(toolbar.hwnd()), "restoring the preference must not show an inactive input method");
  toolbar.SetVisible(true);
  Check(IsWindowVisible(toolbar.hwnd()), "the restored preference must apply when the IME is selected again");
  const int button_x = static_cast<int>(std::lround(294 * scale));
  const int button_y = static_cast<int>(std::lround(376 * scale));
  Mouse(toolbar.hwnd(), WM_LBUTTONDOWN, x, y);
  SendMessageW(toolbar.hwnd(), WM_DPICHANGED, MAKELONG(144, 144), 0);
  RightClickMenu(toolbar.hwnd(), button_x, button_y, true);
  Check(!IsWindowVisible(toolbar.hwnd()) && GetCapture() != toolbar.hwnd() && actions == 0,
        "right-clicking a control during a DPI-changing drag must hide and cancel without clicking it");
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
  TestDragKeepsFullToolbarAtEveryEdge();
  TestRightClickHideSurvivesFocusChanges();
  return failures;
}
