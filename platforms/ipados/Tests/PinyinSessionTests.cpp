#include "PinyinSession.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int failures = 0;
void Check(bool value, const char* message) {
  if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void Type(maodou::Session& session, const std::string& input) {
  for (char letter : input)
    session.Apply(maodou::Action::Key, std::string(1, letter));
}
}

int RunSessionTests(RimeApi* api) {
  using maodou::Action;
  maodou::Session session(api);
  for (const auto& example : {std::pair{"nihao", u8"你好"},
                              {"jintiantianqihenhao", u8"今天天气很好"},
                              {"ipad", "iPad"}, {"izzztest", "izzztest"}}) {
    session.Apply(Action::Clear);
    Type(session, example.first);
    auto snapshot = session.Read();
    Check(!snapshot.preedit.empty() && !snapshot.candidates.empty(),
          "typed letters must produce real preedit and selectable candidates");
    Check(!snapshot.candidates.empty() && snapshot.candidates[0] == example.second,
          "pinyin and exact English names must have the expected first candidate");
    auto update = session.Apply(Action::Select, "", 0);
    Check(update.text == example.second && update.snapshot.preedit.empty(),
          "touch selection must commit the real engine candidate");
  }
  Type(session, "nihao");
  Check(session.Apply(Action::Space).text == u8"你好",
        "Space must select the first candidate while composing");
  Check(session.Apply(Action::Space).text == " ",
        "Space must insert a literal space when not composing");
  Type(session, "nihao");
  Check(session.Apply(Action::Literal, u8"，").text == u8"你好，",
        "a literal symbol must commit the composition before inserting itself");
  Type(session, "nihao");
  auto update = session.Apply(Action::ToggleMode);
  Check(update.text == u8"你好" && update.snapshot.ascii,
        "changing to English must commit the pending Chinese candidate");
  Check(session.Apply(Action::Key, "A").text == "A",
        "English mode must insert the requested letter case directly");
  session.Apply(Action::ToggleMode);
  Type(session, "nihao");
  for (int i = 0; i < 5; ++i)
    Check(!session.Apply(Action::Backspace).delete_backward,
          "deleting uncommitted pinyin must not delete host text");
  Check(session.Read().preedit.empty() &&
            session.Apply(Action::Backspace).delete_backward,
        "deleting with an empty composition must delete host text once");
  Check(session.Apply(Action::Return).text == "\n",
        "Return must insert a newline when not composing");
  Type(session, "nihao");
  Check(session.Apply(Action::Return).text == "nihao",
        "Return while composing must keep Rime's raw-text commit behavior");
  Type(session, "shi");
  const auto first = session.Read();
  Check(first.next_page, "a common pinyin must expose real candidate paging");
  const auto second = session.Apply(Action::NextPage).snapshot;
  Check(second.previous_page && second.candidates != first.candidates,
        "Next must load the engine's next candidate page");
  Check(session.Apply(Action::PreviousPage).snapshot.candidates == first.candidates,
        "Previous must restore the original candidate page");
  Check(session.Apply(Action::Select, "", 9999).text.empty(),
        "a stale candidate index must never commit an unrelated word");
  session.Apply(Action::Clear);
  Check(session.Read().preedit.empty(), "dismissal must clear the composition");
  return failures;
}
