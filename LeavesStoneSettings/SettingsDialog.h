#pragma once

#include <windows.h>

#include "SettingsStore.h"

namespace leaves_stone::settings {

int ShowSettingsDialog(HINSTANCE instance, SettingsStore* store);

}  // namespace leaves_stone::settings
