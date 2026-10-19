#pragma once

#include <rime_api.h>
#include <cstddef>
#include <string>
#include <vector>

namespace maodou {
enum class Action { Key, Space, Return, Backspace, ToggleMode, Literal,
                    Select, NextPage, PreviousPage, Clear };
struct Snapshot {
  std::string preedit;
  std::vector<std::string> candidates;
  size_t selected = 0;
  bool ascii = false;
  bool previous_page = false;
  bool next_page = false;
};
struct Update {
  Snapshot snapshot;
  std::string text;
  bool delete_backward = false;
};

// All access to an instance must stay on its owning serial queue. Runtime
// initialization and filesystem policy belong to the platform bridge.
class Session {
 public:
  explicit Session(RimeApi* api);
  ~Session();
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Snapshot Read() const;
  Update Apply(Action action, const std::string& text = "", size_t index = 0);
 private:
  std::string DrainCommit();
  RimeApi* api_;
  RimeSessionId id_ = 0;
};
}  // namespace maodou
