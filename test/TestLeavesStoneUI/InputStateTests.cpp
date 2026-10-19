#include <iostream>
#include <string>

#include "WeaselTSF/LeavesStoneInputState.h"

namespace {
int failures = 0;

void Check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}
}  // namespace

int RunInputStateTests() {
  using namespace leaves_stone::input_state;

  auto transition =
      PlanModeSwitch(Mode::Chinese, Mode::English, false,
                     CompositionPolicy::Commit);
  Check(transition.action == CompositionAction::None,
        "an empty Chinese to English switch must not edit text");
  Check(!transition.show_candidates && transition.pass_keys_through,
        "English mode must hide candidates and pass keys through");

  transition = PlanModeSwitch(Mode::Chinese, Mode::English, true,
                              CompositionPolicy::Commit);
  Check(transition.action == CompositionAction::Commit,
        "commit policy must preserve active composition when entering English");

  transition = PlanModeSwitch(Mode::Chinese, Mode::English, true,
                              CompositionPolicy::Cancel);
  Check(transition.action == CompositionAction::Cancel,
        "cancel policy must explicitly discard active composition");

  transition = PlanModeSwitch(Mode::English, Mode::Chinese, false,
                              CompositionPolicy::Commit);
  Check(transition.action == CompositionAction::None &&
            transition.show_candidates && !transition.pass_keys_through,
        "returning to Chinese must restore candidate handling");

  transition = PlanModeSwitch(Mode::English, Mode::English, true,
                              CompositionPolicy::Commit);
  Check(transition.action == CompositionAction::None,
        "a redundant mode notification must never commit twice");
  return failures;
}
