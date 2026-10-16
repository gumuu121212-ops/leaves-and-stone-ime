#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
int failures = 0;

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

fs::path FindDataRoot() {
  std::vector<fs::path> candidates = {
      fs::current_path() / "output" / "data",
      fs::path(__FILE__).parent_path().parent_path().parent_path() / "output" /
          "data",
  };
  for (const auto& candidate : candidates) {
    if (fs::exists(candidate / "weasel.yaml"))
      return candidate;
  }
  return candidates.front();
}

std::string ReadRequired(const fs::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    Expect(false, "missing packaged config: " + path.string());
    return {};
  }
  std::ostringstream contents;
  contents << stream.rdbuf();
  return contents.str();
}

bool Matches(const std::string& value, const std::string& pattern) {
  return std::regex_search(
      value, std::regex(pattern, std::regex_constants::icase |
                                    std::regex_constants::multiline));
}

size_t MatchCount(const std::string& value, const std::string& pattern) {
  const std::regex expression(pattern, std::regex_constants::icase |
                                           std::regex_constants::multiline);
  return static_cast<size_t>(
      std::distance(std::sregex_iterator(value.begin(), value.end(), expression),
                    std::sregex_iterator()));
}

void RunConfigTests() {
  const auto data_root = FindDataRoot();
  const auto defaults = ReadRequired(data_root / "default.yaml");
  const auto schema = ReadRequired(data_root / "leaves_stone.schema.yaml");
  const auto dictionary = ReadRequired(data_root / "leaves_stone.dict.yaml");
  const auto theme = ReadRequired(data_root / "leaves_stone_theme.yaml");
  const auto weasel = ReadRequired(data_root / "weasel.yaml");

  Expect(MatchCount(defaults, R"(^\s*-\s*schema:\s*leaves_stone\s*$)") == 1,
         "default.yaml must expose only the leaves_stone schema");
  Expect(Matches(schema, R"(schema_id:\s*leaves_stone)"),
         "schema id must be leaves_stone");
  Expect(Matches(schema, R"(name:\s*ascii_mode)"),
         "English ASCII passthrough must be available");
  Expect(Matches(schema, R"(name:\s*simplification)"),
         "Simplified Chinese conversion switch must be available");
  Expect(Matches(schema, R"(ascii_composer)"),
         "schema must use the standard ASCII composer");
  Expect(Matches(schema, R"(dictionary:\s*leaves_stone)"),
         "schema must use the bundled local dictionary");
  Expect(Matches(schema, R"(page_size:\s*5\s*$)"),
         "candidate page size must default to 5");
  Expect(Matches(dictionary, R"(name:\s*leaves_stone)"),
         "dictionary must have the Leaves & Stone identity");
  Expect(Matches(theme, R"(allowed_counts:\s*\[\s*5\s*,\s*7\s*,\s*9\s*\])"),
         "theme must allow only candidate counts 5, 7, and 9");
  Expect(Matches(theme, R"(invalid_count_fallback:\s*5\s*$)"),
         "invalid candidate counts must fall back to 5");
  Expect(Matches(theme, R"(orientation:\s*horizontal\s*$)"),
         "theme must default to horizontal layout");
  Expect(Matches(theme, R"(animation_enabled:\s*true\s*$)"),
         "theme animation must default on");
  Expect(Matches(theme, R"(decoration_enabled:\s*true\s*$)"),
         "theme decoration must default on");
  Expect(Matches(weasel, R"(horizontal:\s*true\s*$)"),
         "Weasel host layout must be horizontal");
  Expect(Matches(weasel, R"(inline_preedit:\s*false\s*$)"),
         "inline preedit must be disabled");

  std::string packaged = defaults + schema + dictionary + theme + weasel;
  std::transform(packaged.begin(), packaged.end(), packaged.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  const std::vector<std::string> forbidden_settings = {
      "http://", "https://", "network:", "update_url:", "telemetry:",
      "cloud:"};
  for (const std::string& forbidden : forbidden_settings) {
    Expect(packaged.find(forbidden) == std::string::npos,
           "packaged config must not contain " + forbidden);
  }
}
}  // namespace

int main(int argc, char* argv[]) {
  if (argc == 3 && std::string(argv[1]) == "--filter" &&
      std::string(argv[2]) != "Config") {
    return 0;
  }
  RunConfigTests();
  if (failures == 0)
    std::cout << "ConfigTests: PASS\n";
  return failures == 0 ? 0 : 1;
}
