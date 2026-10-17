# Leaves & Stone IME development

Leaves & Stone currently tracks the Weasel 0.17.4 source snapshot (`9cc96e2`) with librime and plum pinned to the commits recorded by that tag.

## Prerequisites

- Visual Studio 2022 with Desktop development with C++
- Windows 10/11 SDK and SignTool
- CMake
- Boost 1.84.0, with `BOOST_ROOT` pointing at the extracted directory
- NSIS 3
- Git
- clang-format 18.1.6 or newer

Verify the current shell without printing the rest of its environment:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/verify-toolchain.ps1
```

## Build and test

Build the Debug binaries used for development:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-local.ps1 -Configuration Debug -Platform x64
```

Build the Win32/x64 Release binaries and installer:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-local.ps1 -Configuration Release -Platform All
```

Weasel's upstream build wrapper emits runtime binaries under `output/` and installers under `output/archives/`. GitHub Actions uses the same wrapper, so local and CI builds follow the same path.

## Package, sign and smoke test

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package.ps1 -SkipBuild
powershell -ExecutionPolicy Bypass -File scripts/new-test-certificate.ps1
powershell -ExecutionPolicy Bypass -File scripts/package.ps1 -Sign -Thumbprint <thumbprint>
powershell -ExecutionPolicy Bypass -File scripts/smoke-install.ps1
```

`package.ps1` refuses to pack when a `.pfx`, `.p12`, `.key` or `.pem` file exists anywhere in the
repository, and writes `SHA256SUMS.txt` plus `manifest.json` next to the installer copy in
`output/artifacts/`. `smoke-install.ps1` installs, repairs and uninstalls, then checks that the
user dictionary under `%APPDATA%\LeavesAndStone\Rime` survives; it must run from an elevated
shell because the installer writes to HKLM and Program Files.

Signing never uses a key stored in this repository. The generated test certificate keeps its
private key outside the tree; see [testing-certificate.md](testing-certificate.md) for trust and
removal steps.

## Contract tests are shallow

`test/Test*Contract.ps1` only assert that files exist and that a few marker strings survive in the
sources. They catch accidental deletions, not regressions: a gutted renderer still passes as long
as the strings remain. Behaviour is covered by the C++ tests in `test/TestLeavesStoneUI/`, which
require a real build.

If the machine policy is AllSigned, run these scripts through
`powershell -ExecutionPolicy Bypass -File <script>`. This host runs Windows PowerShell 5.1 and has
no `pwsh`, so the `pwsh` command line in other documents will not work here.


