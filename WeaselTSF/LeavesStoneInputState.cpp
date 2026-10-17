// WeaselTSF compiles with a precompiled header, so every source in this project
// has to start with stdafx.h or the build fails with C1010.
#include "stdafx.h"
#include "LeavesStoneInputState.h"

namespace leaves_stone::input_state {

Transition PlanModeSwitch(Mode current,
                          Mode requested,
                          bool has_active_composition,
                          CompositionPolicy policy) {
  Transition transition;
  transition.mode = requested;
  transition.show_candidates = requested == Mode::Chinese;
  transition.pass_keys_through = requested == Mode::English;
  if (current == requested || !has_active_composition)
    return transition;
  transition.action = policy == CompositionPolicy::Commit
                          ? CompositionAction::Commit
                          : CompositionAction::Cancel;
  return transition;
}

}  // namespace leaves_stone::input_state
