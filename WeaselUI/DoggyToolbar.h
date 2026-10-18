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

// Status bar with the mascot lying on it. Two complete frames are swapped in
// place: eyes closed while idle, eyes open while the pointer is on the bar.
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
  enum class Hot { None, Mode, Rest, Skin, Settings };

  static LRESULT CALLBACK WindowProc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam);
  LRESULT Handle(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  void ReloadMetrics();
  void ApplyPlacement();
  void Render();
  void SetAwake(bool awake);
  void PollPointer();
  bool PointOnToolbar(int x, int y) const;
  Hot HitTest(int x, int y) const;
  ToolbarAction ActionFor(Hot hot) const;
  RECT SourceRectToWindow(int x, int y, int width, int height) const;

  HINSTANCE instance_ = nullptr;
  HWND hwnd_ = nullptr;
  ThemeAssets assets_;
  std::function<void(ToolbarAction)> handler_;
  Hot hot_ = Hot::None;
  bool awake_ = false;
  bool pointer_inside_ = false;
  bool tracking_mouse_ = false;
  ULONGLONG pointer_changed_at_ = 0;
  float scale_ = 1.0f;
  RECT window_rect_{0, 0, 0, 0};
};

}  // namespace leaves_stone
