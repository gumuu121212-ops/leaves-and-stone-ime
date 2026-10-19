#pragma once

namespace leaves_stone::input_state {

enum class Mode { Chinese, English };
enum class CompositionPolicy { Commit, Cancel };
enum class CompositionAction { None, Commit, Cancel };

struct Transition {
  Mode mode = Mode::Chinese;
  CompositionAction action = CompositionAction::None;
  bool show_candidates = true;
  bool pass_keys_through = false;
};

Transition PlanModeSwitch(Mode current,
                          Mode requested,
                          bool has_active_composition,
                          CompositionPolicy policy);

}  // namespace leaves_stone::input_state
