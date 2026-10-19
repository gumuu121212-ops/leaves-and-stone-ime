#pragma once

#include <windows.h>
#include <functional>
#include <utility>

namespace leaves_stone {

// Retry focus on the original TSF thread after ActivateEx returns. No worker
// thread, simulated keystroke, or access to an inactive text service is needed.
class DeferredToolbarActivation {
 public:
  ~DeferredToolbarActivation() { Stop(); }
  DeferredToolbarActivation() = default;
  DeferredToolbarActivation(const DeferredToolbarActivation&) = delete;
  DeferredToolbarActivation& operator=(const DeferredToolbarActivation&) = delete;

  void Start(std::function<bool()> ready) {
    Stop();
    callback_ = std::move(ready);
    hwnd_ = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0,
                            HWND_MESSAGE, nullptr, nullptr, nullptr);
    if (!hwnd_) { callback_ = {}; return; }
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    previous_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc)));
    PostMessageW(hwnd_, kCheck, 0, 0);
  }
  void Stop() {
    ++generation_;
    callback_ = {};
    attempts_ = 0;
    if (hwnd_) {
      KillTimer(hwnd_, kTimer);
      DestroyWindow(hwnd_);
      hwnd_ = nullptr;
    }
  }

 private:
  static constexpr UINT kCheck = WM_APP + 0x512;
  static constexpr UINT_PTR kTimer = 1;
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message,
                                     WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<DeferredToolbarActivation*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, message, wparam, lparam);
    if (message == kCheck || (message == WM_TIMER && wparam == kTimer)) {
      if (!self->callback_) return 0;
      const unsigned generation = self->generation_;
      auto callback = self->callback_;
      const bool ready = callback();
      if (self->generation_ != generation || self->hwnd_ != hwnd) return 0;
      if (ready || ++self->attempts_ >= 20) {
        KillTimer(hwnd, kTimer);
        self->callback_ = {};
      } else {
        SetTimer(hwnd, kTimer, 150, nullptr);
      }
      return 0;
    }
    return CallWindowProcW(self->previous_, hwnd, message, wparam, lparam);
  }
  HWND hwnd_ = nullptr;
  WNDPROC previous_ = nullptr;
  unsigned attempts_ = 0;
  unsigned generation_ = 0;
  std::function<bool()> callback_;
};

}  // namespace leaves_stone
