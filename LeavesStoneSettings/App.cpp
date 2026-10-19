#include <windows.h>

#include "SettingsDialog.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  leaves_stone::settings::SettingsStore store;
  return leaves_stone::settings::ShowSettingsDialog(instance, &store);
}
