#include <windows.h>
#include <iostream>

#include "WeaselUI/DoggyToolbar.h"

int RunToolbarVisibilityTests() {
  int failures = 0;
  const auto check = [&](bool ok, const char* message) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
  };
  leaves_stone::DoggyToolbar toolbar(GetModuleHandleW(nullptr));
  if (!toolbar.Create([](leaves_stone::ToolbarAction) {})) {
    check(false, "toolbar must load test resources and create its real window");
    return failures;
  }
  check(!IsWindowVisible(toolbar.hwnd()),
        "placement during creation must not show the toolbar before FocusIn");
  toolbar.SetVisible(true);
  check(IsWindowVisible(toolbar.hwnd()), "FocusIn must show the toolbar");
  toolbar.SetVisible(false);
  SendMessageW(toolbar.hwnd(), WM_TIMER, 1, 0);
  check(!IsWindowVisible(toolbar.hwnd()),
        "pointer polling must not reshow a toolbar after FocusOut");
  toolbar.SetVisible(true);
  check(IsWindowVisible(toolbar.hwnd()), "returning to the IME must show it again");
  toolbar.Destroy();
  return failures;
}
