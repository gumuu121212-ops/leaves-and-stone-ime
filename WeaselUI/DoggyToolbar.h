#pragma once

#include <windows.h>

#include <functional>

#include "LeavesStone/ThemeAssets.h"
#include "ToolbarPlacement.h"

namespace leaves_stone {

enum class ToolbarAction {
  ToggleMode,
  ToggleRest,
  OpenSkinPicker,
  OpenSettings,
};

// The same drawing path is used for the real layered window and its previews.
bool DrawDoggyToolbarArtwork(HDC target, const ThemeAssets& assets,
                             bool awake, bool head_only, int width, int height);

// Status bar with the mascot lying on it. Two complete frames are swapped in
// place: eyes closed while idle, eyes open while the pointer is on the bar.
// Dragging to an edge hides the capsule and leaves a rotated head; hovering
// only wakes it, and pulling inward restores the complete toolbar.
class DoggyToolbar {
 public:
  explicit DoggyToolbar(HINSTANCE instance);
  ~DoggyToolbar();

  DoggyToolbar(const DoggyToolbar&) = delete;
  DoggyToolbar& operator=(const DoggyToolbar&) = delete;

  bool Create(std::function<void(ToolbarAction)> handler);
  void Destroy();
  void SetAsciiMode(bool ascii);
  void SetVisible(bool visible);
  HWND hwnd() const { return hwnd_; }

 private:
  enum class Hot { None, Mode, Rest, Skin, Settings };
  enum class DockStage { None, Hide, Peek };

  static LRESULT CALLBACK WindowProc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam);
  LRESULT Handle(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  void ReloadMetrics(UINT dpi = 0);
  void ApplyPlacement();
  void ApplyBounds();
  void ReflowForDisplay(UINT dpi = 0);
  RECT ContentRect() const;
  POINT HeadCenter() const;
  void CancelPointer();
  void BeginPointer(int x, int y);
  void MovePointer(int x, int y);
  void FinishPointer(int x, int y);
  void Dock(ToolbarEdge edge);
  void AdvanceDock();
  void CompleteDock();
  bool HeadOnly() const;
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
  bool button_down_ = false;
  bool dragging_ = false;
  bool undocked_this_drag_ = false;
  Hot pressed_ = Hot::None;
  POINT press_point_{};
  RECT press_window_{};
  ToolbarEdge dock_edge_ = ToolbarEdge::None;
  DockStage dock_stage_ = DockStage::None;
  RECT dock_from_{};
  RECT dock_to_{};
  RECT dock_final_{};
  ULONGLONG dock_started_at_ = 0;
  bool metrics_pending_ = false;
  RECT work_area_{};
  ULONGLONG pointer_changed_at_ = 0;
  float scale_ = 1.0f;
  RECT window_rect_{0, 0, 0, 0};
};

}  // namespace leaves_stone
