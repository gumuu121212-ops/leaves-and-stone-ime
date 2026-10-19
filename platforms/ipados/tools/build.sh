#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
swift tools/make-icon.swift "$PWD"
python3 tools/generate_project.py
mkdir -p dist
DEVICE="$(xcrun simctl list devices available -j | python3 -c '
import json,sys
devices=json.load(sys.stdin)["devices"]
for runtime in sorted(devices,reverse=True):
    match=next((d for d in devices[runtime] if "iPad" in d["name"] and d.get("isAvailable")),None)
    if match:
        print(match["udid"]); break
else: raise SystemExit("No available iPad simulator")
')"
xcrun simctl boot "$DEVICE" || true
xcrun simctl bootstatus "$DEVICE" -b
set +e
xcodebuild -project Xiaomaodou.xcodeproj -scheme Xiaomaodou -configuration Debug \
  -destination "platform=iOS Simulator,id=$DEVICE" -derivedDataPath DerivedData \
  -resultBundlePath .build/KeyboardTests.xcresult -parallel-testing-enabled NO \
  CODE_SIGNING_ALLOWED=NO ONLY_ACTIVE_ARCH=YES test
TEST_RESULT=$?
set -e
xcrun xcresulttool export attachments --path .build/KeyboardTests.xcresult --output-path dist/previews
if [ "$TEST_RESULT" -ne 0 ]; then exit "$TEST_RESULT"; fi
xcodebuild -project Xiaomaodou.xcodeproj -scheme Xiaomaodou -configuration Release \
  -destination 'generic/platform=iOS' -archivePath .build/Xiaomaodou.xcarchive \
  CODE_SIGNING_ALLOWED=NO archive
python3 tools/package.py
