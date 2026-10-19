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

  leaves_stone::CaretPositionState stable;
  Rect position{};
  const auto start_only = [](CaretRange range, Rect& r) {
    if (range != CaretRange::CompositionStart) return false;
    r = {1698, 1056, 1698, 1092}; return true;
  };
  check(stable.Resolve(true, start_only, position) && position.left == 1698,
        "a new composition must allow a start-only layout as its initial anchor");
  for (int caret : {1792, 1812, 1754}) {
    check(stable.Resolve(true, [&](CaretRange range, Rect& r) {
      if (range != CaretRange::Selection) return false;
      r = {caret, 1056, caret, 1092}; return true;
    }, position) && position.left == caret,
          "valid layout must follow typing and backspace without a debounce");
    for (int pending = 0; pending < 3; ++pending) {
      check(!stable.Resolve(true, start_only, position) && position.left == caret,
            "transient layout loss must not jump from the real caret back to "
            "the composition start");
    }
  }
  check(stable.Resolve(true, [](CaretRange range, Rect& r) {
    if (range != CaretRange::Selection) return false;
    r = {1698, 1100, 1698, 1136}; return true;
  }, position) && position.left == 1698 && position.top == 1100,
        "stabilization must still accept a real wrap to the next input line");
  check(!stable.Resolve(true, [](CaretRange, Rect&) { return false; }, position)
            && position.left == 1698 && position.top == 1100,
        "total layout loss must preserve the last accepted wrapped position");

  stable.Reset();
  check(stable.Resolve(true, start_only, position) && position.left == 1698 &&
            position.top == 1056,
        "ending a composition must re-enable the initial start fallback");
  check(stable.Resolve(true, [](CaretRange range, Rect& r) {
    if (range != CaretRange::CompositionEnd) return false;
    r = {1826, 1056, 1826, 1092}; return true;
  }, position) && position.left == 1826,
        "a composition-end position must also establish a precise anchor");
  check(!stable.Resolve(true, start_only, position) && position.left == 1826,
        "layout loss after an end anchor must not revert to the start");
  check(stable.Resolve(false, start_only, position) && position.left == 1698,
        "non-inline preedit must keep using its composition-start anchor");
  return failures;
}
