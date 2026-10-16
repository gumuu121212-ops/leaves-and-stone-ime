#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "LeavesStoneSettings/SettingsStore.h"

namespace fs = std::filesystem;

namespace {
int failures = 0;

void Check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

fs::path UniqueDirectory() {
  const auto value = std::to_wstring(GetTickCount64());
  return fs::temp_directory_path() / (L"leaves-stone-settings-test-" + value);
}
}  // namespace

int RunSettingsStoreTests() {
  const fs::path root = UniqueDirectory();
  const fs::path path = root / L"leaves_stone_settings.yaml";
  leaves_stone::settings::SettingsStore store(path);

  const auto defaults = store.Load();
  Check(defaults.candidate_count == 5,
        "settings must default to five candidates");
  Check(defaults.text_size == leaves_stone::settings::TextSize::Medium,
        "settings must default to medium text");
  Check(defaults.orientation ==
            leaves_stone::settings::Orientation::Horizontal,
        "settings must default to horizontal orientation");
  Check(defaults.animation_enabled && defaults.decoration_enabled,
        "animation and decoration must default on");

  auto chosen = defaults;
  chosen.candidate_count = 9;
  chosen.text_size = leaves_stone::settings::TextSize::Large;
  chosen.orientation = leaves_stone::settings::Orientation::Vertical;
  chosen.animation_enabled = false;
  chosen.decoration_enabled = false;
  Check(store.Save(chosen), "settings save must use an atomic replacement");
  Check(!fs::exists(path.wstring() + L".tmp"),
        "atomic save must not leave its temporary file behind");
  Check(store.Load() == chosen, "all five settings must round-trip");

  chosen.candidate_count = 6;
  Check(store.Save(chosen), "invalid candidate count must be normalized");
  Check(store.Load().candidate_count == 5,
        "candidate count must be restricted to 5, 7, or 9");

  auto observed = store.Load();
  leaves_stone::settings::SettingsStore writer(path);
  chosen.candidate_count = 7;
  chosen.animation_enabled = true;
  Check(writer.Save(chosen), "a second process must be able to save settings");
  Check(store.ReloadIfChanged(&observed),
        "the running renderer must hot-reload a changed settings file");
  Check(observed.candidate_count == 7 && observed.animation_enabled,
        "hot reload must expose the new validated settings");

  {
    std::ofstream corrupt(path, std::ios::binary | std::ios::trunc);
    corrupt << "candidate_count: definitely-not-a-number\n";
  }
  const auto recovered = store.Load();
  Check(recovered == leaves_stone::settings::UserSettings{},
        "corrupt settings must fall back to safe defaults");
  Check(fs::exists(path.wstring() + L".corrupt.bak"),
        "corrupt settings must be backed up for recovery");

  std::error_code ignored;
  fs::remove_all(root, ignored);
  return failures;
}
