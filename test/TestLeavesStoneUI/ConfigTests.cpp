#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <rime_api.h>

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
  // std::regex_constants::multiline is a search flag, not a constructor option,
  // so it cannot be combined with icase here. These patterns anchor with ^ and $
  // per line, so match each line instead.
  const std::regex expression(pattern, std::regex_constants::icase);
  std::istringstream lines(value);
  std::string line;
  while (std::getline(lines, line)) {
    if (std::regex_search(line, expression))
      return true;
  }
  return false;
}

size_t MatchCount(const std::string& value, const std::string& pattern) {
  const std::regex expression(pattern, std::regex_constants::icase);
  size_t count = 0;
  std::istringstream lines(value);
  std::string line;
  while (std::getline(lines, line)) {
    if (std::regex_search(line, expression))
      ++count;
  }
  return count;
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
  Expect(Matches(theme, R"(file:\s*leaves_stone_settings\.yaml\s*$)"),
         "theme must declare the product-owned user settings file");
  Expect(Matches(theme, R"(reload:\s*next_candidate_frame\s*$)"),
         "settings changes must apply on the next candidate frame");
  Expect(Matches(weasel, R"(horizontal:\s*true\s*$)"),
         "Weasel host layout must be horizontal");
  Expect(Matches(weasel, R"(inline_preedit:\s*true\s*$)"),
         "inline preedit must be enabled");

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

void RunRimeIntegrationTest() {
  const auto data_root = FindDataRoot();
  const auto unique_suffix = std::to_string(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const auto user_root =
      fs::temp_directory_path() / ("leaves-stone-rime-test-" + unique_suffix);
  fs::create_directories(user_root);

  const auto shared_data = data_root.string();
  const auto user_data = user_root.string();
  const auto prebuilt_data = (data_root / "build").string();
  RimeApi* rime = rime_get_api();
  Expect(rime != nullptr, "librime API must be available");
  if (!rime)
    return;

  RIME_STRUCT(RimeTraits, traits);
  traits.shared_data_dir = shared_data.c_str();
  traits.user_data_dir = user_data.c_str();
  traits.prebuilt_data_dir = prebuilt_data.c_str();
  traits.distribution_name = "Leaves & Stone Tests";
  traits.distribution_code_name = "leaves_stone_tests";
  traits.distribution_version = "1.0";
  traits.app_name = "leaves_stone.config_tests";

  rime->setup(&traits);
  rime->initialize(nullptr);
  if (rime->start_maintenance(True))
    rime->join_maintenance_thread();

  const RimeSessionId session = rime->create_session();
  Expect(session != 0, "librime must create a test session");
  if (session) {
    Expect(rime->select_schema(session, "leaves_stone") == True,
           "librime must load the leaves_stone schema");
    rime->set_option(session, "simplification", True);
    Expect(rime->simulate_key_sequence(session, "jintiantianqihenhao") == True,
           "full-pinyin key sequence must be accepted");

    bool found_simplified_sentence = false;
    RimeCandidateListIterator iterator = {0};
    if (rime->candidate_list_begin(session, &iterator)) {
      while (rime->candidate_list_next(&iterator)) {
        if (iterator.candidate.text &&
            std::string(iterator.candidate.text) == u8"今天天气很好") {
          found_simplified_sentence = true;
          break;
        }
      }
      rime->candidate_list_end(&iterator);
    }
    Expect(found_simplified_sentence,
           "jintiantianqihenhao must produce the Simplified Chinese candidate "
           "今天天气很好");
    rime->destroy_session(session);
  }

  rime->finalize();
  std::error_code cleanup_error;
  fs::remove_all(user_root, cleanup_error);
}
}  // namespace

int RunSceneLayoutTests();
int RunRendererSmokeTests();
int RunSettingsStoreTests();
int RunInputStateTests();

int main(int argc, char* argv[]) {
  const std::string filter =
      argc == 3 && std::string(argv[1]) == "--filter" ? argv[2] : "";
  if (filter.empty() || filter == "Config") {
    RunConfigTests();
    RunRimeIntegrationTest();
  }
  if (filter.empty() || filter == "SceneLayout")
    failures += RunSceneLayoutTests();
  if (filter.empty() || filter == "Renderer")
    failures += RunRendererSmokeTests();
  if (filter.empty() || filter.find("Settings") != std::string::npos)
    failures += RunSettingsStoreTests();
  if (filter.empty() || filter.find("InputState") != std::string::npos)
    failures += RunInputStateTests();
  if (failures == 0)
    std::cout << "TestLeavesStoneUI: PASS\n";
  return failures == 0 ? 0 : 1;
}
