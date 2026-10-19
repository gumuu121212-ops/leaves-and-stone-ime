#include "PinyinSession.h"

#include <stdexcept>

namespace maodou {
Session::Session(RimeApi* api) : api_(api) {
  if (!api_ || !(id_ = api_->create_session()))
    throw std::runtime_error("Cannot create the offline pinyin session");
  if (!api_->select_schema(id_, "leaves_stone")) {
    api_->destroy_session(id_);
    id_ = 0;
    throw std::runtime_error("The bundled pinyin dictionary is unavailable");
  }
  api_->set_option(id_, "simplification", True);
  api_->set_option(id_, "ascii_mode", False);
}

Session::~Session() {
  if (id_) api_->destroy_session(id_);
}

Snapshot Session::Read() const {
  Snapshot result;
  RIME_STRUCT(RimeContext, context);
  if (api_->get_context(id_, &context)) {
    if (context.composition.preedit)
      result.preedit = context.composition.preedit;
    result.selected = static_cast<size_t>(context.menu.highlighted_candidate_index);
    result.previous_page = context.menu.page_no > 0;
    result.next_page = context.menu.num_candidates > 0 && !context.menu.is_last_page;
    for (int i = 0; i < context.menu.num_candidates; ++i)
      result.candidates.emplace_back(context.menu.candidates[i].text
                                        ? context.menu.candidates[i].text : "");
    api_->free_context(&context);
  }
  RIME_STRUCT(RimeStatus, status);
  if (api_->get_status(id_, &status)) {
    result.ascii = status.is_ascii_mode != False;
    api_->free_status(&status);
  }
  return result;
}

std::string Session::DrainCommit() {
  RIME_STRUCT(RimeCommit, commit);
  if (!api_->get_commit(id_, &commit)) return {};
  const std::string text = commit.text ? commit.text : "";
  api_->free_commit(&commit);
  return text;
}

Update Session::Apply(Action action, const std::string& text, size_t index) {
  Update result;
  const Snapshot before = Read();
  const bool composing = !before.preedit.empty();
  switch (action) {
    case Action::Key:
      if (before.ascii) {
        result.text = text;
      } else if (text.size() == 1) {
        const bool handled = api_->process_key(
            id_, static_cast<unsigned char>(text[0]), 0) != False;
        result.text = DrainCommit();
        if (!handled) result.text += text;
      }
      break;
    case Action::Literal:
      if (composing) api_->commit_composition(id_);
      result.text = DrainCommit() + text;
      break;
    case Action::Space:
      if (composing) api_->process_key(id_, ' ', 0);
      else result.text = " ";
      break;
    case Action::Return:
      if (composing) api_->process_key(id_, 0xff0d, 0);
      else result.text = "\n";
      break;
    case Action::Backspace:
      if (composing) api_->process_key(id_, 0xff08, 0);
      else result.delete_backward = true;
      break;
    case Action::ToggleMode:
      if (composing) api_->commit_composition(id_);
      result.text = DrainCommit();
      api_->set_option(id_, "ascii_mode", before.ascii ? False : True);
      break;
    case Action::Select:
      if (index < before.candidates.size())
        api_->select_candidate_on_current_page(id_, index);
      break;
    case Action::NextPage:
      if (before.next_page) api_->process_key(id_, 0xff56, 0);
      break;
    case Action::PreviousPage:
      if (before.previous_page) api_->process_key(id_, 0xff55, 0);
      break;
    case Action::Clear:
      api_->clear_composition(id_);
      break;
  }
  result.text += DrainCommit();
  result.snapshot = Read();
  return result;
}
}  // namespace maodou
