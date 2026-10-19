#pragma once

#include <cstdint>
#include <functional>
#include <utility>

namespace leaves_stone {

// The focused session can arrive before the floating toolbar has been created.
class ToolbarFocusState {
 public:
  void Bind(std::function<void(bool)> callback) {
    callback_ = std::move(callback);
    Notify();
  }
  void FocusIn(uint32_t session) {
    if (!session) return;
    focused_session_ = session;
    Notify();
  }
  bool FocusOut(uint32_t session) {
    if (!session || session != focused_session_) return false;
    focused_session_ = 0;
    Notify();
    return true;
  }
  void Reset() {
    focused_session_ = 0;
    Notify();
  }
 private:
  void Notify() {
    if (callback_) callback_(focused_session_ != 0);
  }
  uint32_t focused_session_ = 0;
  std::function<void(bool)> callback_;
};

}  // namespace leaves_stone
