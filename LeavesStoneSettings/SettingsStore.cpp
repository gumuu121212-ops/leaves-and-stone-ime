#include "SettingsStore.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <LeavesStoneProduct.h>

namespace leaves_stone::settings {
namespace {

std::string Trim(std::string value) {
  const auto not_space = [](unsigned char value) { return !std::isspace(value); };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
              value.end());
  return value;
}

bool ParseBool(const std::string& value, bool* parsed) {
  if (value == "true") {
    *parsed = true;
    return true;
  }
  if (value == "false") {
    *parsed = false;
    return true;
  }
  return false;
}

const char* TextSizeName(TextSize value) {
  switch (value) {
    case TextSize::Small:
      return "small";
    case TextSize::Large:
      return "large";
    default:
      return "medium";
  }
}

const char* OrientationName(Orientation value) {
  return value == Orientation::Vertical ? "vertical" : "horizontal";
}

}  // namespace

bool UserSettings::operator==(const UserSettings& other) const {
  return candidate_count == other.candidate_count &&
         text_size == other.text_size && orientation == other.orientation &&
         animation_enabled == other.animation_enabled &&
         decoration_enabled == other.decoration_enabled;
}

UserSettings Validate(UserSettings settings) {
  if (settings.candidate_count != 5 && settings.candidate_count != 7 &&
      settings.candidate_count != 9) {
    settings.candidate_count = 5;
  }
  return settings;
}

std::filesystem::path DefaultSettingsPath() {
  wchar_t app_data[MAX_PATH] = {};
  const DWORD length = GetEnvironmentVariableW(L"APPDATA", app_data, MAX_PATH);
  std::filesystem::path root =
      length > 0 && length < MAX_PATH ? std::filesystem::path(app_data)
                                     : std::filesystem::temp_directory_path();
  return root / product::kUserDataSubdir / L"leaves_stone_settings.yaml";
}

SettingsStore::SettingsStore(std::filesystem::path path)
    : path_(std::move(path)) {}

bool SettingsStore::Read(UserSettings* settings, bool* corrupt) const {
  if (!settings || !corrupt)
    return false;
  *corrupt = false;
  std::ifstream stream(path_, std::ios::binary);
  if (!stream)
    return false;

  std::map<std::string, std::string> values;
  std::string line;
  while (std::getline(stream, line)) {
    line = Trim(line);
    if (line.empty() || line.front() == '#')
      continue;
    const size_t separator = line.find(':');
    if (separator == std::string::npos) {
      *corrupt = true;
      return false;
    }
    values[Trim(line.substr(0, separator))] = Trim(line.substr(separator + 1));
  }

  const char* required[] = {"candidate_count", "text_size", "orientation",
                            "animation_enabled", "decoration_enabled"};
  for (const char* key : required) {
    if (!values.count(key)) {
      *corrupt = true;
      return false;
    }
  }

  UserSettings parsed;
  try {
    size_t consumed = 0;
    parsed.candidate_count = std::stoi(values["candidate_count"], &consumed);
    if (consumed != values["candidate_count"].size())
      throw std::invalid_argument("trailing candidate count");
  } catch (...) {
    *corrupt = true;
    return false;
  }

  if (values["text_size"] == "small")
    parsed.text_size = TextSize::Small;
  else if (values["text_size"] == "medium")
    parsed.text_size = TextSize::Medium;
  else if (values["text_size"] == "large")
    parsed.text_size = TextSize::Large;
  else {
    *corrupt = true;
    return false;
  }

  if (values["orientation"] == "horizontal")
    parsed.orientation = Orientation::Horizontal;
  else if (values["orientation"] == "vertical")
    parsed.orientation = Orientation::Vertical;
  else {
    *corrupt = true;
    return false;
  }

  if (!ParseBool(values["animation_enabled"], &parsed.animation_enabled) ||
      !ParseBool(values["decoration_enabled"], &parsed.decoration_enabled)) {
    *corrupt = true;
    return false;
  }

  *settings = Validate(parsed);
  return true;
}

UserSettings SettingsStore::Load() {
  UserSettings settings;
  bool corrupt = false;
  if (!Read(&settings, &corrupt) && corrupt)
    BackupCorruptFile();
  RememberWriteTime();
  return Validate(settings);
}

bool SettingsStore::Save(const UserSettings& raw_settings) {
  const UserSettings settings = Validate(raw_settings);
  std::error_code error;
  std::filesystem::create_directories(path_.parent_path(), error);
  if (error)
    return false;

  std::filesystem::path temporary = path_;
  temporary += L".tmp";
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream)
      return false;
    stream << "# Leaves & Stone user settings\n"
           << "candidate_count: " << settings.candidate_count << "\n"
           << "text_size: " << TextSizeName(settings.text_size) << "\n"
           << "orientation: " << OrientationName(settings.orientation) << "\n"
           << "animation_enabled: "
           << (settings.animation_enabled ? "true" : "false") << "\n"
           << "decoration_enabled: "
           << (settings.decoration_enabled ? "true" : "false") << "\n";
    stream.flush();
    if (!stream)
      return false;
  }

  if (!MoveFileExW(temporary.c_str(), path_.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::filesystem::remove(temporary, error);
    return false;
  }
  RememberWriteTime();
  return true;
}

bool SettingsStore::ReloadIfChanged(UserSettings* settings) {
  if (!settings)
    return false;
  // Compare content instead of relying on last_write_time alone: Windows
  // updates file times on a ~15.6 ms clock, so a save landing in the same tick
  // as the previous one leaves the timestamp unchanged and the edit would be
  // ignored. The file is a few hundred bytes, so reading it per candidate frame
  // is cheaper than losing a user's change.
  UserSettings loaded;
  bool corrupt = false;
  if (!Read(&loaded, &corrupt)) {
    // Missing or unreadable: keep the settings already in use rather than
    // snapping back to defaults on a transient read failure.
    if (corrupt)
      BackupCorruptFile();
    return false;
  }
  const UserSettings validated = Validate(loaded);
  const bool changed = validated != *settings;
  *settings = validated;
  RememberWriteTime();
  return changed;
}

void SettingsStore::RememberWriteTime() {
  std::error_code error;
  last_write_time_ = std::filesystem::last_write_time(path_, error);
  has_write_time_ = !error;
}

void SettingsStore::BackupCorruptFile() const {
  std::filesystem::path backup = path_;
  backup += L".corrupt.bak";
  MoveFileExW(path_.c_str(), backup.c_str(),
              MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

}  // namespace leaves_stone::settings
