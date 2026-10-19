#include <rime_api.h>
#include <filesystem>
#include <iostream>
#include <string>

int RunSessionTests(RimeApi* api);

int main(int argc, char** argv) {
  if (argc != 4) return 2;
  const std::string stage = (std::filesystem::path(argv[3]) / "build").string();
  std::filesystem::create_directories(stage);
  std::filesystem::create_directories(argv[2]);
  auto* api = rime_get_api();
  RIME_STRUCT(RimeTraits, traits);
  traits.shared_data_dir = argv[1];
  traits.user_data_dir = argv[2];
  traits.prebuilt_data_dir = stage.c_str();
  traits.staging_dir = stage.c_str();
  traits.app_name = "rime.xiaomaodou_dictionary_builder";
  traits.log_dir = "";
  traits.min_log_level = 2;
  api->setup(&traits);
  api->initialize(nullptr);
  if (api->start_maintenance(True)) api->join_maintenance_thread();
  for (const char* file : {"default.yaml", "leaves_stone.schema.yaml",
                           "leaves_stone.table.bin", "leaves_stone.prism.bin",
                           "leaves_stone_english.table.bin"}) {
    if (!std::filesystem::exists(std::filesystem::path(stage) / file)) {
      std::cerr << "Missing compiled dictionary: " << file << '\n';
      api->finalize(); return 3;
    }
  }
  const int failures = RunSessionTests(api);
  std::cout << "Rime version: " << api->get_version() << '\n';
  api->finalize();
  return failures ? 1 : 0;
}
