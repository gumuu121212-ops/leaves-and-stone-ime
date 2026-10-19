#pragma once
#include <windows.h>
#include <algorithm>

namespace leaves_stone {

inline RECT ClampToolbarWindow(RECT window, const RECT& content,
                               const RECT& work) {
  const LONG width = window.right - window.left;
  const LONG height = window.bottom - window.top;
  const LONG min_x = work.left - content.left;
  const LONG min_y = work.top - content.top;
  const LONG max_x = (std::max)(min_x, work.right - content.right);
  const LONG max_y = (std::max)(min_y, work.bottom - content.bottom);
  window.left = std::clamp(window.left, min_x, max_x);
  window.top = std::clamp(window.top, min_y, max_y);
  window.right = window.left + width;
  window.bottom = window.top + height;
  return window;
}

}  // namespace leaves_stone
