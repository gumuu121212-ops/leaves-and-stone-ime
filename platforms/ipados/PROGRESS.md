# iPad development record

Base: 3b6fb16bb860b4fba7725e00fcc2bacdb24f3973, feature/leaves-stone-ime.

Confirmed: real system third-party keyboard; primary use is touch. Mac availability question is pending.

Read spec and plan in docs/superpowers before continuing. Do not modify Windows runtime or regenerate dog artwork.

2026-10-19: Read Apple custom-keyboard documentation, verified the pinned librime dynamic XCFramework SHA-256 and device/simulator/macOS slices. The portable C++ session now passes real librime tests on Windows in an isolated profile: full pinyin, long sentence, iPad/raw candidates, Space/touch commits, Return, host backspace, mode switching and paging.

Ruling: Use the same release's static XCFramework, not its dynamic variant. Inspection of the device Mach-O shows the dynamic variant lacks MH_APP_EXTENSION_SAFE; static linking avoids distributing that unsafe-marked dylib and lets the extension's own application-extension build enforce API restrictions. Keep dependencies pinned and carry all notices. Still need native Apple compilation, UIKit/controller work and signing. Do not claim the Windows session test validates UIKit or an iPad installation.

Task 1 complete: 720a272 macOS run 39214645784, job 117935562724 passed the real native session tests and fresh-profile input against bundled prebuilt data, without runtime dictionary compilation.

Task 2 in progress: native app, extension and hosted XCTest targets generated. First Apple compile found Swift imports applyAction as apply; fixed in e5201e1. Simulator run 39216205218 / job 117940535432 demonstrated the expected RED: the empty surface failed layout, candidate-tap and repeat assertions; both initial-key and old-document commit tests passed against real Rime. Full surface/controller implementation now replaces those stubs, plus host setup, exact original dog layers, serialized destruction, and device packaging script. Further Apple compile/test and a fresh review remain mandatory.

Ruling: execute inline in the existing feature checkout, using the separate platforms/ipados tree. The app thread's managed-worktree context points at a different repository; a new managed worktree would target the wrong project. The unrelated Windows runtime and artwork remain unchanged.

Ruling: production keyboard composition stays inside its header; only final commits enter the host proxy. Recheck document identifier and context at both command and insertion boundaries, in addition to the serial service's epoch check.

Independent whole-change review found stale candidate index, held-delete after focus changes, queued mode semantics, selected-text omission and dog-top clipping. Run 39218151189 / job 117946806219 reproduced all five with real simulator assertions; normal controller touch selection, first keys and the SwiftUI setup window passed. Actual setup and four production-surface renders were exported and inspected. Two further focus-boundary cases (silent focus move during repeat, synchronous host focus move during insertion) were added in f51caba for RED verification. Fixes in the working tree have been independently rechecked; Apple GREEN and arm64 archive remain pending. Fixes include queue revisions, queue-side case/punctuation resolution, selectedText/selection callbacks, cancellation generation, and checking document ID after host writes.

Visual inspection: the 320pt number toggle needed a smaller 16pt label to avoid truncating 123; the setup call-to-action now uses white on dark brown. The dog header baseline moves down 16pt to retain all fur. Original source artwork is unchanged.

RED verified for the last two focus cases: f980d22 run 39219330202 / job 117950670201 produced SAFEb after synchronous host focus change and deleted SAFE after silent focus change. The tests therefore exercise real failures before fixes. All seven findings have production fixes and independent read-only re-review with no remaining material finding. Full GREEN simulator run, SDK-matched simulator choice, and unsigned device archive are the next gate. Mac/signing availability is still not answered; no device installation has been claimed.

