#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/prepare.py
mkdir -p .build/bin
MAC_LIBRARY="$(python3 - <<'PY'
import pathlib, plistlib
p=pathlib.Path('Vendor/librime-static.xcframework')
info=plistlib.loads((p/'Info.plist').read_bytes())
item=next(i for i in info['AvailableLibraries'] if i['SupportedPlatform']=='macos')
print((p/item['LibraryIdentifier']/item['LibraryPath']).resolve())
PY
)"
COMMON=(-std=c++17 -I Vendor/RimeHeaders -I Native Native/PinyinSession.cpp
        Tests/PinyinSessionTests.cpp "-Wl,-force_load,$MAC_LIBRARY" -liconv -lz)
clang++ "${COMMON[@]}" tools/prebuild.cpp -o .build/bin/prebuild
.build/bin/prebuild "$PWD/.build/RimeSource" "$PWD/.build/DictionaryBuilder" "$PWD/Resources/Rime"
clang++ "${COMMON[@]}" tools/session-test-main.cpp -o .build/bin/session-tests
.build/bin/session-tests "$PWD/Resources/Rime" "$PWD/.build/FirstBootProfile" --prebuilt-only
python3 - <<'PY'
from pathlib import Path
assert not list(Path('.build/FirstBootProfile/build').glob('*.table.bin')), 'Extension must use prebuilt dictionaries'
print('Fresh-profile input without runtime dictionary compilation: PASS')
PY
