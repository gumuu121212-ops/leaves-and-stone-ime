#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
swift tools/make-icon.swift "$PWD"
python3 tools/generate_project.py
mkdir -p dist
SDK_VERSION="$(xcrun --sdk iphonesimulator --show-sdk-version)"
DEVICE="$(xcrun simctl list devices available -j | python3 -c '
import json,sys
devices=json.load(sys.stdin)["devices"]
matching=".iOS-"+sys.argv[1].replace(".","-")
runtimes=sorted(devices,reverse=True)
runtimes=sorted(runtimes,key=lambda r:not r.endswith(matching))
for runtime in runtimes:
    options=sorted(devices[runtime],key=lambda d:("mini" not in d["name"].lower(),d["name"]))
    match=next((d for d in options if "iPad" in d["name"] and d.get("isAvailable")),None)
    if match:
        print(match["udid"]); break
else: raise SystemExit("No available iPad simulator")
' "$SDK_VERSION")"
xcodebuild -project Xiaomaodou.xcodeproj -scheme Xiaomaodou -configuration Debug \
  -destination "platform=iOS Simulator,id=$DEVICE" -derivedDataPath DerivedData \
  CODE_SIGNING_ALLOWED=NO ONLY_ACTIVE_ARCH=YES build-for-testing
xcrun simctl boot "$DEVICE" || true
xcrun simctl bootstatus "$DEVICE" -b
set +e
xcodebuild -project Xiaomaodou.xcodeproj -scheme Xiaomaodou -configuration Debug \
  -destination "platform=iOS Simulator,id=$DEVICE" -derivedDataPath DerivedData \
  -resultBundlePath .build/KeyboardTests.xcresult -parallel-testing-enabled NO \
  CODE_SIGNING_ALLOWED=NO ONLY_ACTIVE_ARCH=YES test-without-building
TEST_RESULT=$?
set -e
xcrun xcresulttool export attachments --path .build/KeyboardTests.xcresult --output-path dist/previews
if [ "$TEST_RESULT" -ne 0 ]; then exit "$TEST_RESULT"; fi
xcodebuild -project Xiaomaodou.xcodeproj -scheme Xiaomaodou -configuration Release \
  -destination 'generic/platform=iOS' -archivePath .build/Xiaomaodou.xcarchive \
  CODE_SIGNING_ALLOWED=NO archive
python3 tools/package.py
