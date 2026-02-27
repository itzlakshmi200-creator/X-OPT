# X-OPT Engine

```
 ██╗  ██╗      ██████╗ ██████╗ ████████╗
 ╚██╗██╔╝     ██╔═══██╗██╔══██╗╚══██╔══╝
  ╚███╔╝      ██║   ██║██████╔╝   ██║   
  ██╔██╗      ██║   ██║██╔═══╝    ██║   
 ██╔╝ ██╗     ╚██████╔╝██║        ██║   
 ╚═╝  ╚═╝      ╚═════╝ ╚═╝        ╚═╝   
```

**The ultimate realtime PC optimisation engine for low-end hardware.**  
Premium iOS-inspired dark UI. No bloat. Pure performance.

---

## Features

| Panel       | What it does |
|-------------|-------------|
| **Boost**   | High Performance power plan, 1ms timer resolution, CPU priority separation, Game Mode, disable SuperFetch/animations/GameBar, Network Nagle-off |
| **Clean**   | One-tap wipe of `%TEMP%`, `C:\Windows\Temp`, Prefetch, and DNS cache |
| **Launch**  | Browse + launch any `.exe` with `HIGH_PRIORITY_CLASS` + `THREAD_PRIORITY_HIGHEST` |
| **Phonk**   | Background MP3/WAV player with animated visualiser and volume slider |

---

## UI Design

Inspired by iOS — pure black backgrounds, iOS-blue accents, smooth animated toggles, custom slider widgets with glow effects, spring-animated tab selector, and toast notifications. Built with **Dear ImGui + DirectX 11**.

---

## Building Locally

### Requirements
- Windows 10/11
- [CMake ≥ 3.20](https://cmake.org/)
- [Visual Studio 2022](https://visualstudio.microsoft.com/) with C++ workload
- [vcpkg](https://vcpkg.io/) (bootstrapped automatically via CI)

### Steps

```powershell
# 1. Bootstrap vcpkg
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics

# 2. Configure
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="./vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows

# 3. Build
cmake --build build --config Release

# 4. Run (as Administrator for full effect)
.\build\release\X-OPT.exe
```

---

## GitHub Actions CI/CD

Push a tag like `v1.0.0` and the workflow will:
1. Compile with MSVC x64 Release
2. Upload `X-OPT.exe` as a build artifact
3. Create a GitHub Release with the binary attached

```bash
git tag v1.0.0
git push origin v1.0.0
```

---

## Notes

- Run as **Administrator** — some optimisations (bcdedit, sc, registry writes) require elevated privileges
- The app requests elevation automatically via the embedded UAC manifest
- `Kill Explorer` hides the taskbar — toggle it off to bring it back
- `High-Res Timer` calls `timeBeginPeriod(1)` — reduces scheduling overhead and input lag
- All changes are **reversible** by toggling off

---

## License

MIT — free for everyone.
