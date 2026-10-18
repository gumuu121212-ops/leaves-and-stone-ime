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

}  // namespace leaves_stone
