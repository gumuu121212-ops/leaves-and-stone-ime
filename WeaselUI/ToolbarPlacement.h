#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdlib>

namespace leaves_stone {
enum class ToolbarEdge { None, Left, Top, Right, Bottom };

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

inline ToolbarEdge NearbyToolbarEdge(const RECT& content, const RECT& work,
                                      int distance) {
  const int gaps[] = {std::abs(content.left - work.left),
                      std::abs(content.top - work.top),
                      std::abs(work.right - content.right),
                      std::abs(work.bottom - content.bottom)};
  const ToolbarEdge edges[] = {ToolbarEdge::Left, ToolbarEdge::Top,
                               ToolbarEdge::Right, ToolbarEdge::Bottom};
  ToolbarEdge result = ToolbarEdge::None;
  int closest = distance + 1;
  for (int i = 0; i < 4; ++i) {
    if (gaps[i] < closest) { closest = gaps[i]; result = edges[i]; }
  }
  return result;
}

inline RECT ToolbarPeekRect(POINT center, SIZE head, const RECT& work,
                            ToolbarEdge edge) {
  LONG x = std::clamp(center.x - head.cx / 2, work.left,
                      (std::max)(work.left, work.right - head.cx));
  LONG y = std::clamp(center.y - head.cy / 2, work.top,
                      (std::max)(work.top, work.bottom - head.cy));
  if (edge == ToolbarEdge::Left) x = work.left - head.cx / 10;
  if (edge == ToolbarEdge::Right) x = work.right - head.cx + head.cx / 10;
  if (edge == ToolbarEdge::Top) y = work.top - head.cy / 10;
  if (edge == ToolbarEdge::Bottom) y = work.bottom - head.cy + head.cy / 10;
  return {x, y, x + head.cx, y + head.cy};
}
}  // namespace leaves_stone
