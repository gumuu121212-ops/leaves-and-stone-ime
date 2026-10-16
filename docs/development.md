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

