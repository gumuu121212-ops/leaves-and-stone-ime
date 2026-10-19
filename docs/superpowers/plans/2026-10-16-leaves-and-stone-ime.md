# Leaves & Stone IME Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an installable Windows 10/11 Chinese pinyin IME named Leaves & Stone with English passthrough and a fixed, frameless 3D paper-cut candidate UI based on the approved illustration.

**Architecture:** Fork Weasel 0.17.4 at commit `9cc96e2` and retain its TSF, IPC, installer, and librime integration. Add an isolated Leaves & Stone product identity and registry/data paths, then introduce a pure candidate-scene layout model and a Windows renderer that draws separate raised candidate cards plus layered botanical/architectural assets inside one transparent hit-tested top-level window.

**Tech Stack:** C++17, Windows TSF/COM, WTL/ATL/MFC as used by Weasel, DirectWrite/GDI+/WIC, librime, YAML, MSBuild/Visual Studio, PowerShell, NSIS, SignTool.

**Spec:** `docs/superpowers/specs/2026-10-16-leaves-and-stone-ime-design.md`

## Global Constraints

- Support Windows 10 x64 and Windows 11 x64, including input into 32-bit host applications.
- Ship only Simplified Chinese full pinyin plus English passthrough in version 1.0.
- Keep the fixed Leaves & Stone theme; no theme import, accounts, cloud dictionary, telemetry, advertisements, or network access.
- Preserve Weasel GPL-3.0 notices and upstream attribution; publish all derivative source under GPL-3.0.
- Use `%APPDATA%\LeavesAndStone\Rime` for user data and `Software\LeavesAndStone\IME` for product registry state.
- Do not reuse Weasel TSF profile GUIDs, candidate UI GUID, installer identity, registry keys, executable display names, or Start Menu shortcuts.
- Do not draw a shared visible background around all candidates; each candidate and control is visually independent.
- Never log committed user text or complete composition strings.
- Test-certificate trust installation must require explicit user confirmation and must be removable independently.

## Review Focus

- Candidate geometry at 125%, 150%, and 200% DPI must remain within the active monitor work area; Task 4 adds deterministic boundary tests.
- Empty, single, five, seven, nine, and overlong candidate lists must not crash or overlap; Task 4 adds model tests for each case.
- Renderer recreation after a device/GPU reset must restore all resources without leaking handles; Task 5 adds a repeated create/destroy smoke test and manual WER/GDI-handle check.
- Switching between Chinese and English while a composition is active must end or preserve composition according to the selected command without losing committed text; Task 6 adds state-transition tests.
- Install, repair, upgrade, and uninstall must not delete the user dictionary unless explicitly selected; Task 7 adds clean-VM and retained-data integration checks.

---

## File Structure

- `WeaselUI/LeavesStone/SceneTypes.h`: renderer-independent scene and hit-test types.
- `WeaselUI/LeavesStone/SceneLayout.h/.cpp`: deterministic candidate-card layout, DPI scaling, anchoring, and work-area clamping.
- `WeaselUI/LeavesStone/ThemeConfig.h/.cpp`: validated fixed-theme configuration and accessibility fallbacks.
- `WeaselUI/LeavesStone/SceneRenderer.h/.cpp`: DirectWrite/GDI+/WIC drawing and resource lifecycle.
- `WeaselUI/LeavesStone/ThemeAssets.h/.cpp`: compiled resource IDs and decoded bitmap access.
- `WeaselUI/WeaselPanel.cpp`: choose Leaves & Stone scene path, paint it, and route mouse hit testing.
- `WeaselTSF/Globals.h/.cpp`, `WeaselTSF/Register.cpp`, `WeaselTSF/CandidateList.cpp`: product-specific GUIDs, registration strings, and candidate UI identity.
- `RimeWithWeasel/WeaselUtility.cpp`: isolated user/shared data paths and registry path.
- `LeavesStoneSettings/`: small standalone settings executable.
- `output/data/`: fixed pinyin schema, default configuration, and theme settings.
- `output/install.nsi`: branded install/repair/uninstall and optional user-data removal.
- `resource/leaves-stone/`: transparent decorative PNG resources and application icons.
- `scripts/`: toolchain verification, build, test-signing, packaging, and smoke-test scripts.
- `test/TestLeavesStoneUI/`: C++ tests for config, scene layout, hit testing, and state transitions.

### Task 1: Import and Reproduce the Upstream Weasel Baseline

**Files:**
- Modify: repository root by merging upstream tag `0.17.4`
- Create: `scripts/verify-toolchain.ps1`
- Create: `scripts/build-local.ps1`
- Create: `docs/development.md`

**Interfaces:**
- Consumes: Visual Studio C++ desktop workload, Windows SDK, CMake, Boost, Git, NSIS.
- Produces: `scripts/build-local.ps1 -Configuration <Debug|Release> -Platform <Win32|x64>` and an unchanged upstream baseline build in `output/`.

- [ ] **Step 1: Record the clean upstream baseline**

Add remote `upstream`, fetch tag `0.17.4`, merge commit `9cc96e2` with `--allow-unrelated-histories`, and initialize all submodules recursively. Preserve the existing `docs/` files and visual reference.

- [ ] **Step 2: Write the toolchain preflight script**

Implement `scripts/verify-toolchain.ps1` with `Get-LeavesStoneToolchainStatus() -> PSCustomObject`. It must report paths and versions for MSBuild, Windows SDK/SignTool, CMake, Boost, NSIS, Git, and clang-format, and exit nonzero only when a required build dependency is absent.

- [ ] **Step 3: Verify preflight failure and success paths**

Run the script once with a deliberately invalid `BOOST_ROOT` and expect a nonzero exit naming `BOOST_ROOT`; run it with the resolved dependency path and expect exit code 0 with no secret or environment dump.

- [ ] **Step 4: Add the reproducible build wrapper**

Implement `scripts/build-local.ps1` to call the upstream build commands without mutating global environment variables. `-Configuration Debug` must build test projects; `-Configuration Release` must produce Win32 and x64 binaries used by the installer.

- [ ] **Step 5: Build and run the untouched baseline tests**

Run `powershell -ExecutionPolicy Bypass -File scripts/build-local.ps1 -Configuration Debug -Platform x64` and the upstream test executables from `output`. Expected: build succeeds and all existing tests exit 0 before Leaves & Stone changes.

- [ ] **Step 6: Document the exact local setup**

Write `docs/development.md` with prerequisite versions, one-command build/test examples, and the location of generated binaries. Do not include machine-specific absolute paths.

- [ ] **Step 7: Commit**

Commit the upstream import, wrapper scripts, and development documentation.

### Task 2: Create an Independent Leaves & Stone Product Identity

**Files:**
- Modify: `WeaselTSF/Globals.h`
- Modify: `WeaselTSF/Globals.cpp`
- Modify: `WeaselTSF/Register.cpp`
- Modify: `WeaselTSF/CandidateList.cpp`
- Modify: `RimeWithWeasel/WeaselUtility.cpp`
- Modify: `WeaselSetup/WeaselSetup.rc`
- Modify: `WeaselDeployer/WeaselDeployer.rc`
- Modify: `WeaselServer/WeaselServer.rc`
- Modify: `output/install.nsi`
- Modify: `resource/resource.rc`
- Create: `include/LeavesStoneProduct.h`
- Create: `test/TestLeavesStoneUI/ProductIdentityTests.cpp`

**Interfaces:**
- Consumes: upstream TSF registration and setup flow from Task 1.
- Produces: constants in namespace `leaves_stone::product`, including `kDisplayName`, `kRegistryRoot`, `kUserDataSubdir`, `kTextServiceClsid`, `kLanguageProfileGuid`, and `kCandidateUiGuid`.

- [ ] **Step 1: Write failing product-identity tests**

Add tests asserting that all display strings equal `Leaves & Stone`, registry root equals `Software\\LeavesAndStone\\IME`, user data resolves beneath `%APPDATA%\\LeavesAndStone\\Rime`, and the three product GUIDs differ from upstream Weasel GUIDs and from one another.

- [ ] **Step 2: Run the focused test and verify failure**

Build and run `TestLeavesStoneUI.exe --filter ProductIdentity`. Expected: compile or assertion failure because `LeavesStoneProduct.h` does not exist.

- [ ] **Step 3: Add the product constants and generated GUIDs**

Create `include/LeavesStoneProduct.h` as the single source of truth. Generate GUIDs once with `New-Guid`, commit their literal values, and never generate them at install time.

- [ ] **Step 4: Replace registration, data-path, and resource identities**

Route TSF registration, candidate UI identity, registry access, default `%APPDATA%` location, version resources, icons, shortcuts, uninstall key, and installer names through the new constants. Keep required `Powered by RIME` attribution in legal/about text.

- [ ] **Step 5: Run identity and upstream regression tests**

Run the focused test, then all Debug tests. Expected: all pass; a repository search for user-visible `小狼毫` and product registry `Software\\Rime\\Weasel` returns only upstream attribution/migration documentation, never active Leaves & Stone registration code.

- [ ] **Step 6: Commit**

Commit the independent product identity and registration isolation.

### Task 3: Ship the Fixed Simplified-Pinyin Configuration

**Files:**
- Create: `output/data/leaves_stone.schema.yaml`
- Create: `output/data/leaves_stone.dict.yaml`
- Create: `output/data/default.yaml`
- Modify: `output/data/weasel.yaml`
- Create: `output/data/leaves_stone_theme.yaml`
- Create: `test/TestLeavesStoneUI/ConfigTests.cpp`

**Interfaces:**
- Consumes: librime schema loading and product paths from Tasks 1–2.
- Produces: schema id `leaves_stone`, Simplified Chinese full-pinyin candidates, English passthrough, fixed candidate counts `5|7|9`, and theme flags `animation_enabled` and `decoration_enabled`.

- [ ] **Step 1: Write failing configuration tests**

Add tests that load the packaged YAML and assert: only `leaves_stone` is in `schema_list`; page size defaults to 5 and accepts only 5, 7, or 9; simplified conversion is enabled; English ASCII mode is available; network/update settings are absent; the theme default is horizontal, animated, and decorated.

- [ ] **Step 2: Verify the tests fail**

Run `TestLeavesStoneUI.exe --filter Config`. Expected: failure because the packaged schema and theme files are missing.

- [ ] **Step 3: Add the minimal schema and dictionary wiring**

Package a full-pinyin schema using librime's standard processors/segmentors/translators, Simplified Chinese output, local user dictionary learning, and English passthrough. Reuse compatible upstream/open Rime data only with its license and attribution included.

- [ ] **Step 4: Add fixed UI defaults and validation**

Set horizontal five-candidate mode, inline preedit off, status notifications minimal, and theme defaults matching the approved design. Invalid candidate counts must fall back to 5.

- [ ] **Step 5: Run schema console and configuration tests**

Feed `jintiantianqihenhao` to `rime_api_console.exe`; expect at least one Simplified Chinese candidate sequence. Run focused and full tests; all must pass.

- [ ] **Step 6: Commit**

Commit the fixed pinyin data and configuration validation.

### Task 4: Build the Renderer-Independent 3D Candidate Scene Model

**Files:**
- Create: `WeaselUI/LeavesStone/SceneTypes.h`
- Create: `WeaselUI/LeavesStone/SceneLayout.h`
- Create: `WeaselUI/LeavesStone/SceneLayout.cpp`
- Create: `WeaselUI/LeavesStone/ThemeConfig.h`
- Create: `WeaselUI/LeavesStone/ThemeConfig.cpp`
- Create: `test/TestLeavesStoneUI/SceneLayoutTests.cpp`
- Modify: `WeaselUI/WeaselUI.vcxproj`
- Modify: `test/TestLeavesStoneUI/TestLeavesStoneUI.vcxproj`

**Interfaces:**
- Consumes: `Context`, `UIStyle`, monitor work area, anchor rectangle, and theme YAML from Task 3.
- Produces: `leaves_stone::CandidateScene BuildCandidateScene(const SceneInput&)`; `std::optional<HitTarget> HitTest(const CandidateScene&, PointF)`.

- [ ] **Step 1: Write failing scene-layout tests**

Cover empty, one, five, seven, nine, and overlong candidate sets; 100/125/150/200% DPI; lower-edge flip above caret; left/right clamping; selected-card elevation of 4 logical pixels; non-overlapping hit regions; and no visible shared background node.

- [ ] **Step 2: Run the focused tests and verify failure**

Run `TestLeavesStoneUI.exe --filter SceneLayout`. Expected: compile failure because `BuildCandidateScene` and `HitTest` are undefined.

- [ ] **Step 3: Define scene types**

Define `SceneInput`, `CandidateCard`, `ControlCard`, `DecorationLayer`, `CandidateScene`, and `HitTarget` using logical device-independent pixels. Keep Windows drawing handles out of these types.

- [ ] **Step 4: Implement deterministic layout**

Implement `BuildCandidateScene` with separate pinyin plaque, candidate cards, circular controls, a transparent outer window bound, 12 logical-pixel gaps, 4-pixel selected elevation, and work-area clamping. Overlong labels truncate visually but retain full candidate text for TSF selection.

- [ ] **Step 5: Implement hit testing**

Implement `HitTest` so transparent gaps and decoration return no target, while cards and controls return stable semantic IDs.

- [ ] **Step 6: Run focused and full tests**

Expected: all layout tests and existing upstream tests pass in Debug x64 and Win32.

- [ ] **Step 7: Commit**

Commit the pure scene model and tests.

### Task 5: Render and Interact With the 3D Paper-Cut Candidate Window

**Files:**
- Create: `WeaselUI/LeavesStone/SceneRenderer.h`
- Create: `WeaselUI/LeavesStone/SceneRenderer.cpp`
- Create: `WeaselUI/LeavesStone/ThemeAssets.h`
- Create: `WeaselUI/LeavesStone/ThemeAssets.cpp`
- Modify: `WeaselUI/WeaselPanel.cpp`
- Modify: `WeaselUI/WeaselPanel.h`
- Modify: `WeaselUI/WeaselUI.vcxproj`
- Create: `resource/leaves-stone/leaf-left.png`
- Create: `resource/leaves-stone/leaf-right.png`
- Create: `resource/leaves-stone/building-terracotta.png`
- Create: `resource/leaves-stone/building-ivory.png`
- Create: `resource/leaves-stone/sky-blue.png`
- Create: `resource/leaves-stone/architectural-lines.png`
- Create: `test/TestLeavesStoneUI/RendererSmokeTests.cpp`

**Interfaces:**
- Consumes: `CandidateScene` and `HitTarget` from Task 4.
- Produces: `SceneRenderer::Render(HDC, const CandidateScene&, const ThemeAssets&)`; `SceneRenderer::DiscardDeviceResources()`; UI callbacks carrying candidate index, previous/next page, language state, or settings action.

- [ ] **Step 1: Create transparent theme assets from the approved reference**

Extract/repaint the reference into six transparent PNG layers. Preserve warm ivory, terracotta, olive/deep green, pale blue, and charcoal; exclude the title text from runtime candidate decoration.

- [ ] **Step 2: Write failing renderer lifecycle and hit-routing tests**

Add tests for resource create/discard/recreate, transparent-gap clicks, candidate click indexes, paging buttons, and decoration never capturing input.

- [ ] **Step 3: Verify focused failure**

Run `TestLeavesStoneUI.exe --filter Renderer`. Expected: compile failure because `SceneRenderer` is absent.

- [ ] **Step 4: Implement theme asset loading and resource lifecycle**

Decode compiled PNG resources with WIC, cache immutable bitmaps, and release device-dependent resources on target loss or DPI change. No filesystem lookup is allowed during normal candidate painting.

- [ ] **Step 5: Implement the paper-cut renderer**

Draw transparent window content in this order: rear decorations, card shadows, card thickness, card faces, selected face, text/labels, front leaves, independent controls. Do not call the upstream whole-panel background drawing path in Leaves & Stone mode.

- [ ] **Step 6: Connect painting, animation, and hit testing**

Update `WeaselPanel` to build a scene on context/style/DPI change, animate initial appearance for 140 ms, route mouse events through `HitTest`, and skip animation when disabled.

- [ ] **Step 7: Verify rendering behavior**

Run focused/full tests, then run a repeated 1,000-cycle create/render/discard smoke test. Expected: no failed HRESULT, stable GDI/USER handle counts, and successful resource recreation after forced discard.

- [ ] **Step 8: Commit**

Commit the themed candidate renderer, assets, and integration.

### Task 6: Add Settings and Input-State Behavior

**Files:**
- Create: `LeavesStoneSettings/LeavesStoneSettings.vcxproj`
- Create: `LeavesStoneSettings/App.cpp`
- Create: `LeavesStoneSettings/SettingsDialog.h`
- Create: `LeavesStoneSettings/SettingsDialog.cpp`
- Create: `LeavesStoneSettings/SettingsStore.h`
- Create: `LeavesStoneSettings/SettingsStore.cpp`
- Create: `test/TestLeavesStoneUI/SettingsStoreTests.cpp`
- Create: `test/TestLeavesStoneUI/InputStateTests.cpp`
- Modify: `weasel.sln`
- Modify: `WeaselTSF/Compartment.cpp`
- Modify: `output/data/leaves_stone_theme.yaml`

**Interfaces:**
- Consumes: validated `ThemeConfig` from Task 4 and TSF/librime state from upstream.
- Produces: `SettingsStore::Load() -> UserSettings`; `SettingsStore::Save(const UserSettings&)`; settings change notification; predictable `Chinese`, `English`, and `Composing` transitions.

- [ ] **Step 1: Write failing settings and input-state tests**

Assert defaults, 5/7/9 validation, atomic save, corrupt-file backup, hot reload, animation/decoration flags, and Chinese→English transitions with empty and active compositions.

- [ ] **Step 2: Verify focused failure**

Run `TestLeavesStoneUI.exe --filter Settings,InputState`. Expected: compile failure because settings APIs are absent.

- [ ] **Step 3: Implement the settings store**

Store only candidate count, text size enum, orientation enum, animation flag, and decoration flag under the product user-data directory. Save via temporary file plus atomic replace; back up invalid YAML before falling back to defaults.

- [ ] **Step 4: Implement the Win32 settings dialog**

Expose the five approved settings, a reset button, and a live preview image. Saving signals the running server to reload without restarting host applications.

- [ ] **Step 5: Normalize Chinese/English state transitions**

Ensure English mode hides candidates and passes keys through. When switching with active composition, follow the configured commit/cancel command exactly and add regression coverage for committed text preservation.

- [ ] **Step 6: Run focused and full tests**

Expected: all tests pass; changing settings while typing updates the next candidate frame without crashing the host.

- [ ] **Step 7: Commit**

Commit settings and input-state behavior.

### Task 7: Build the Installer, Test Signing, Repair, and Uninstall Flow

**Files:**
- Modify: `output/install.nsi`
- Create: `scripts/new-test-certificate.ps1`
- Create: `scripts/sign-build.ps1`
- Create: `scripts/package.ps1`
- Create: `scripts/smoke-install.ps1`
- Create: `docs/testing-certificate.md`
- Create: `docs/install.md`
- Modify: `LICENSE.txt`

**Interfaces:**
- Consumes: signed Release binaries, product constants, settings executable, schema data, and runtime assets from Tasks 2–6.
- Produces: `output/archives/leaves-and-stone-<version>-test-installer.exe`, repair/uninstall entries, and explicit certificate install/remove instructions.

- [ ] **Step 1: Write failing package validation checks**

Add `scripts/smoke-install.ps1 -Mode ValidatePackage` assertions for expected filenames, x86/x64 TSF DLLs, signatures, schema files, theme assets, GPL notices, installer metadata, and absence of active Weasel registry/product strings.

- [ ] **Step 2: Run validation and verify failure**

Expected: failure listing missing branded installer and unsigned product binaries.

- [ ] **Step 3: Implement explicit test-certificate creation**

`new-test-certificate.ps1` creates a code-signing certificate only after an interactive confirmation, exports public certificate and protected PFX to a caller-specified directory, and prints removal instructions without printing passwords.

- [ ] **Step 4: Implement signing and packaging**

`sign-build.ps1` signs all executable binaries and verifies signatures. `package.ps1` invokes NSIS with Leaves & Stone metadata and refuses to package if any required binary is unsigned.

- [ ] **Step 5: Implement install, repair, and uninstall behavior**

Register the product TSF service and startup server, add settings/repair/uninstall shortcuts, preserve user data on repair, and show a checkbox on uninstall for deleting `%APPDATA%\LeavesAndStone\Rime`.

- [ ] **Step 6: Test on clean Windows 10 and Windows 11 VMs**

Verify fresh install, Windows input-list appearance, repair, upgrade over the same version, uninstall with retained data, uninstall with deleted data, and independent removal of the test certificate.

- [ ] **Step 7: Run package validation**

Expected: `smoke-install.ps1 -Mode ValidatePackage` exits 0 and the installer signature chain validates after trusting the test certificate.

- [ ] **Step 8: Commit**

Commit installer, signing, documentation, and package validation.

### Task 8: Cross-Application Acceptance and Test Release

**Files:**
- Create: `docs/qa-matrix.md`
- Create: `docs/privacy.md`
- Create: `docs/third-party-notices.md`
- Create: `CHANGELOG.md`
- Create: `README.md`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: test installer from Task 7.
- Produces: completed compatibility evidence and the distributable 1.0 test-release bundle.

- [ ] **Step 1: Add CI build and test gates**

Build Debug Win32/x64, run all tests, build unsigned Release artifacts for validation, run clang-format checks, and retain logs/artifacts. Do not install certificates or publish releases from pull-request CI.

- [ ] **Step 2: Execute the application matrix**

On Windows 10 and 11, test Notepad, Word, Edge, Chrome, WeChat, and Obsidian at 100/125/150/200% DPI, including single/dual monitor, screen edges, light/dark hosts, Chinese/English transitions, mouse selection, paging, and long candidates.

- [ ] **Step 3: Record failures and fix blockers**

Any crash, lost committed text, missing candidate window, off-screen window, unremovable installation, or user-data deletion is release-blocking. Add a regression test before each code fix.

- [ ] **Step 4: Verify privacy and licensing**

Confirm no network calls, no committed-text logs, GPL source availability, BSD/librime and asset attribution, and complete third-party notices.

- [ ] **Step 5: Build the final test-release bundle**

Package installer, public test certificate, certificate instructions, checksums, README, changelog, licenses, and source archive. Do not include the private key/PFX.

- [ ] **Step 6: Run final verification**

From a clean checkout, rebuild, run all automated tests, validate the package, install on a clean VM, type and commit `今天天气很好`, switch to English and commit `Leaves & Stone`, then uninstall. Record the exact results in `docs/qa-matrix.md`.

- [ ] **Step 7: Commit**

Commit CI, QA evidence, documentation, and the test-release metadata.

