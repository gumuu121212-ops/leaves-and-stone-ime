#pragma once

namespace leaves_stone {

enum class CaretRange { Selection, CompositionEnd, CompositionStart };

// Keep the fallback order independent of COM for regression tests.
template <typename Query, typename Rect>
bool ResolveCaretRect(bool inline_preedit, Query&& query, Rect& result) {
  const CaretRange inline_ranges[] = {CaretRange::Selection,
                                     CaretRange::CompositionEnd,
                                     CaretRange::CompositionStart};
  const CaretRange separate_ranges[] = {CaretRange::CompositionStart,
                                       CaretRange::Selection};
  const auto* ranges = inline_preedit ? inline_ranges : separate_ranges;
  const int count = inline_preedit ? 3 : 2;
  for (int i = 0; i < count; ++i) {
    Rect candidate{};
    if (query(ranges[i], candidate)) {
      result = candidate;
      return true;
    }
  }
  return false;
}

class CaretPositionState {
 public:
  void Reset() { has_precise_inline_position_ = false; }

  template <typename Query, typename Rect>
  bool Resolve(bool inline_preedit, Query&& query, Rect& result) {
    return ResolveCaretRect(inline_preedit, [&](CaretRange range, Rect& rect) {
      // Chromium can lay out the composition start while its new selection/end
      // is still pending. Once we have a real caret, do not jump back to that
      // start: the caller retains its last position until OnLayoutChange.
      if (inline_preedit && has_precise_inline_position_ &&
          range == CaretRange::CompositionStart)
        return false;
      if (!query(range, rect))
        return false;
      if (inline_preedit && range != CaretRange::CompositionStart)
        has_precise_inline_position_ = true;
      return true;
    }, result);
  }

 private:
  bool has_precise_inline_position_ = false;
};

}  // namespace leaves_stone
