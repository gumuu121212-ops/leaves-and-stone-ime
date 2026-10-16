#pragma once

#include <filesystem>

namespace leaves_stone::settings {

enum class TextSize { Small, Medium, Large };
enum class Orientation { Horizontal, Vertical };

struct UserSettings {
  int candidate_count = 5;
  TextSize text_size = TextSize::Medium;
  Orientation orientation = Orientation::Horizontal;
  bool animation_enabled = true;
  bool decoration_enabled = true;

  bool operator==(const UserSettings& other) const;
  bool operator!=(const UserSettings& other) const { return !(*this == other); }
};

UserSettings Validate(UserSettings settings);
std::filesystem::path DefaultSettingsPath();

class SettingsStore {
 public:
  explicit SettingsStore(std::filesystem::path path = DefaultSettingsPath());

  UserSettings Load();
  bool Save(const UserSettings& settings);
  bool ReloadIfChanged(UserSettings* settings);

  const std::filesystem::path& path() const { return path_; }

 private:
  bool Read(UserSettings* settings, bool* corrupt) const;
  void RememberWriteTime();
  void BackupCorruptFile() const;

  std::filesystem::path path_;
  std::filesystem::file_time_type last_write_time_{};
  bool has_write_time_ = false;
};

}  // namespace leaves_stone::settings
