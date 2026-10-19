# iPad development record

Base: 3b6fb16bb860b4fba7725e00fcc2bacdb24f3973, feature/leaves-stone-ime.

Confirmed: real system third-party keyboard; primary use is touch. Mac availability question is pending.

Read spec and plan in docs/superpowers before continuing. Do not modify Windows runtime or regenerate dog artwork.

2026-10-19: Read Apple custom-keyboard documentation, verified the pinned librime dynamic XCFramework SHA-256 and device/simulator/macOS slices. The portable C++ session now passes real librime tests on Windows in an isolated profile: full pinyin, long sentence, iPad/raw candidates, Space/touch commits, Return, host backspace, mode switching and paging.

Ruling: Use the same release's static XCFramework, not its dynamic variant. Inspection of the device Mach-O shows the dynamic variant lacks MH_APP_EXTENSION_SAFE; static linking avoids distributing that unsafe-marked dylib and lets the extension's own application-extension build enforce API restrictions. Keep dependencies pinned and carry all notices. Still need native Apple compilation, UIKit/controller work and signing. Do not claim the Windows session test validates UIKit or an iPad installation.

