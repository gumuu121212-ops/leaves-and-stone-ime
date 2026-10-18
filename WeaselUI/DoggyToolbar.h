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

// Floating status bar anchored to the bottom-right corner of the work area:
// a plush capsule with the mascot leaning on it and four buttons.
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
  void SetState(State state);
  void Wake();
  Hot HitTest(int x, int y) const;
  ToolbarAction ActionFor(Hot hot) const;
  RECT NativeRectToWindow(int x, int y, int width, int height) const;
  bool PointInVisibleArea(int x, int y) const;

  HINSTANCE instance_ = nullptr;
  HWND hwnd_ = nullptr;
  ThemeAssets assets_;
  std::function<void(ToolbarAction)> handler_;
  State state_ = State::Default;
  Hot hot_ = Hot::None;
  Hot pressed_ = Hot::None;
  bool ascii_mode_ = false;
  bool tracking_mouse_ = false;
  bool hover_pending_ = false;
  bool dragging_ = false;
  POINT drag_origin_{0, 0};
  RECT drag_window_{0, 0, 0, 0};
  float scale_ = 1.0f;
  RECT window_rect_{0, 0, 0, 0};
};

}  // namespace leaves_stone
