# PoE2Mouse
Windows-native keyboard and mouse input management research for local co-op. Primary goal: a single physical keyboard and mouse for two characters, preserving native keyboard/mouse interaction when supported. Physical controllers are optional.

## v0.1.2 test lab
Self-contained local two-character movement and action simulator, portable C++20 state engine, profile import/export/reload and Windows packaging. **This is not a PoE2 game adapter and cannot control characters in the actual game.** It only accepts foreground input in its own window.

Download the Windows test package from a successful [Actions run](https://github.com/jkszz231210-ops/poe2mouse/actions) on dev/bootstrap. Unzip all files and run input_lab.exe. See HOME_TEST.md and [中文使用与反馈说明](docs/FEEDBACK.md). Help menu can copy or save a bounded diagnostic report; no automatic logs or uploads. No driver, Python or third-party runtime dependencies for this lab; Windows runtime verification remains pending.

## Build
CMake 3.22+ and C++20 compiler. Windows: Visual Studio 2022 Desktop development with C++.
```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```
On Windows the dist folder contains the portable test application. Every mapped action is configurable in lab.profile; use the Profile menu to import/export/reload.

See [engineering contract](AGENTS.md), [architecture](docs/ARCHITECTURE.md), [status](docs/STATUS.md) and [home test guide](docs/HOME_TEST.md).
