#include <iostream>
#include <vector>

#include "WeaselTSF/CaretPosition.h"

int RunCaretPositionTests() {
  using leaves_stone::CaretRange;
  struct Rect { int left, top, right, bottom; };
  int failures = 0;
  const auto check = [&](bool ok, const char* message) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
  };
  Rect result{};
  for (int caret : {100, 160, 120}) {
    check(leaves_stone::ResolveCaretRect(true, [&](CaretRange range, Rect& r) {
      r = range == CaretRange::Selection ? Rect{caret, 80, caret, 100}
                                         : Rect{100, 80, 100, 100};
      return true;
    }, result) && result.left == caret,
          "inline candidates must track the real caret, including moving back");
  }
  check(leaves_stone::ResolveCaretRect(true, [](CaretRange, Rect& r) {
    r = {-1200, 130, -1200, 150}; return true;
  }, result) && result.top == 130 && result.left == -1200,
        "caret tracking must preserve wrapping and negative monitor coordinates");
  std::vector<CaretRange> attempted;
  check(leaves_stone::ResolveCaretRect(true, [&](CaretRange range, Rect& r) {
    attempted.push_back(range);
    if (range != CaretRange::CompositionEnd) return false;
    r = {220, 80, 220, 100}; return true;
  }, result) && result.left == 220 && attempted.size() == 2 &&
        attempted[0] == CaretRange::Selection,
        "missing selection layout must fall back to the composition end");
  check(leaves_stone::ResolveCaretRect(true, [](CaretRange range, Rect& r) {
    if (range != CaretRange::CompositionStart) return false;
    r = {100, 80, 100, 100}; return true;
  }, result) && result.left == 100,
        "a host without end layout must retain a usable composition-start fallback");
  attempted.clear();
  check(leaves_stone::ResolveCaretRect(false, [&](CaretRange range, Rect& r) {
    attempted.push_back(range); r = {100, 80, 100, 100}; return true;
  }, result) && attempted.front() == CaretRange::CompositionStart,
        "non-inline composition must retain its existing start anchor");
  result = {7, 8, 9, 10};
  check(!leaves_stone::ResolveCaretRect(true, [](CaretRange, Rect&) {
    return false;
  }, result) && result.left == 7 && result.bottom == 10,
        "unavailable layout must not replace the last position with garbage");
  return failures;
}
