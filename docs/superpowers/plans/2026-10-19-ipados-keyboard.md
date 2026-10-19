# iPad Keyboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Produce a real iPad third-party touch keyboard with 小毛豆 artwork and offline full-pinyin input.

**Architecture:** New iPad-only app and keyboard extension under platforms/ipados. A portable C++ Rime session feeds an Objective-C++ bridge and serialized Swift service; UIKit controls submit committed text through UITextDocumentProxy.

**Tech Stack:** iPadOS 15+, UIKit/SwiftUI, Swift 5, Objective-C++, C++17, librime 1.17, Xcode, Python standard-library preparation tools.

**Spec:** ../specs/2026-10-19-ipados-keyboard.md

## Global Constraints

- No changes to Windows runtime, installer or existing artwork.
- Genuine offline full-pinyin; preserve existing schema and exact-name/raw candidates.
- RequestsOpenAccess=false; private extension user dictionary; no input content logging.
- Widths 320, 600, 768 and 1024 pt; no truncated candidates; preserve composition across layout changes.
- Never call an unsigned IPA installable or simulator tests device verification.

## Review Focus

- Late callbacks must not write into another text document.
- Backspace repeat must stop on touch cancellation or keyboard disappearance.
- Initialization must not drop the first letters.
- First-use input must load bundled prebuilt dictionaries without compiling in the extension.
- The extension must statically link an extension-safe engine and contain required notices.

### Task 1: Portable Rime session and pinned resources

**Files:** platforms/ipados/Native/PinyinSession.{h,cpp}, Tests/PinyinSessionTests.cpp, tools/prepare.py, tools/prebuild.cpp, dependencies.json.

**Interfaces:** Session(RimeApi*) owns one leaves_stone session. Apply(Action,string,index) returns Update{snapshot,text,delete_backward}; Read() returns preedit, candidates, selected index, ascii and paging flags.

- [x] Write real-engine tests for nihao, complete sentence, ipad, raw unknown, page navigation, Space, Return, Backspace, literal insertion and switching mode.
- [x] Demonstrate expected failure before implementing the session.
- [x] Implement minimal C++ session; prove tests with the installed librime in an isolated temporary profile.
- [x] Pin framework and data sources with digests; prepare licenses and precompiled dictionaries using the same engine on macOS.

### Task 2: Native extension and touch interface

**Files:** platforms/ipados/Keyboard/{RimeBridge.h,RimeBridge.mm,InputService.swift,KeyboardViewController.swift,KeyboardSurface.swift}, App/XiaomaodouApp.swift, Configuration/*.plist, Tests/KeyboardTests.swift.

**Interfaces:** RimeBridge exposes snapshots and committed text. InputService serializes actions and stamps document epochs; only matching epochs may deliver text to the current proxy. KeyboardSurface emits typed actions and owns no engine state.

- [x] Write tests for serialized initial keys, document-epoch rejection, tap selection, variable widths and repeat cancellation.
- [x] Add UIKit surface with original awake/sleep frames, complete candidates and touch keys.
- [x] Add controller lifecycle and proxy integration, globe and dismiss actions; keep input state outside layout rebuilds.
- [x] Add host setup instructions and a text-entry field for device checking.

### Task 3: Xcode build, review and delivery

**Files:** platforms/ipados/Xiaomaodou.xcodeproj, tools/generate_project.py, tools/build.sh, README.md, .github/workflows/ipados-ci.yml.

- [x] Generate a checked, deterministic native Xcode project with app, keyboard and test targets.
- [ ] Run simulator tests and capture a render from production UI where possible.
- [ ] Build arm64 device product without claiming a missing developer signature exists.
- [ ] Perform a fresh whole-change review and fix material findings.
- [ ] Package source, verified build artifacts, hashes and clear signing/install instructions. Apply user-provided signing only after the app is concrete and required authority is available.

