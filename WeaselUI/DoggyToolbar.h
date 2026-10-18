#pragma once

#include <windows.h>

#include <functional>

#include "LeavesStone/ThemeAssets.h"

namespace leaves_stone {

enum class ToolbarAction {
  ToggleMode,
  ToggleRest,
  OpenSkinPicker,
  OpenSettings,
};

// Compact status strip drawn in the plush palette and parked inside the taskbar
// row: mascot head, mode capsule and three buttons.
class DoggyToolbar {
 public:
  explicit DoggyToolbar(HINSTANCE instance);
  ~DoggyToolbar();

  DoggyToolbar(const DoggyToolbar&) = delete;
  DoggyToolbar& operator=(const DoggyToolbar&) = delete;

  bool Create(std::function<void(ToolbarAction)> handler);
  void Destroy();
  void SetAsciiMode(bool ascii);
  HWND hwnd() const { return hwnd_; }

 private:
  enum class State { Default, Hover, Pressed, Idle };
  enum class Hot { None, Mode, Rest, Skin, Settings };

  static LRESULT CALLBACK WindowProc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam);
  LRESULT Handle(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  void ReloadMetrics();
  void ApplyPlacement();
  void Render();
  Hot HitTest(int x, int y) const;
  ToolbarAction ActionFor(Hot hot) const;
  RECT DipRect(const RECT& dip) const;

  HINSTANCE instance_ = nullptr;
  HWND hwnd_ = nullptr;
  ThemeAssets assets_;
  std::function<void(ToolbarAction)> handler_;
  State state_ = State::Default;
  Hot hot_ = Hot::None;
  Hot pressed_ = Hot::None;
  bool ascii_mode_ = false;
  bool tracking_mouse_ = false;
  float scale_ = 1.0f;
  RECT window_rect_{0, 0, 0, 0};
};

}  // namespace leaves_stone
