#include <rime_api.h>
#include <filesystem>
#include <iostream>
#include <string>

int RunSessionTests(RimeApi* api);

int main(int argc, char** argv) {
  if (argc < 3) { std::cerr << "usage: session-tests shared-data user-data [--prebuilt-only]\n"; return 2; }
  std::filesystem::create_directories(argv[2]);
  const std::string prebuilt = (std::filesystem::path(argv[1]) / "build").string();
  auto* api = rime_get_api();
  RIME_STRUCT(RimeTraits, traits);
  traits.shared_data_dir = argv[1];
  traits.user_data_dir = argv[2];
  traits.prebuilt_data_dir = prebuilt.c_str();
  traits.app_name = "rime.xiaomaodou_ipados_tests";
  traits.log_dir = "";
  traits.min_log_level = 3;
  api->setup(&traits);
  api->initialize(nullptr);
  if (!(argc > 3 && std::string(argv[3]) == "--prebuilt-only") &&
      api->start_maintenance(True))
    api->join_maintenance_thread();
  int result = 1;
  try { result = RunSessionTests(api); }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
  api->finalize();
  if (!result) std::cout << "iPad native Rime session: PASS\n";
  return result ? 1 : 0;
}
