# PoE2Mouse
Windows-native keyboard and mouse input management research for local co-op. Primary goal: a single physical keyboard and mouse for two characters, preserving native keyboard/mouse interaction when supported. Physical controllers are optional.

## v0.1.6 test lab
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
On Windows the dist folder contains the portable test application. Defaults live in lab.default.profile; user imports are saved to lab.profile, which takes precedence and is never shipped in the new archive. Schema 1/2 remain readable; schema 3 adds separate skill slots and modifier chords. Profile > Edit bindings is a native text editor with validation and persistence. See [binding model](docs/BINDINGS.md). Use Help > Check input devices for a read-only device snapshot.

See [engineering contract](AGENTS.md), [architecture](docs/ARCHITECTURE.md), [status](docs/STATUS.md) and [home test guide](docs/HOME_TEST.md).

Optional Windows `device_probe.exe` validates creation/removal of two neutral virtual devices using a user-supplied trusted SDK DLL and driver. See [device probe instructions](docs/DEVICE_PROBE.md). It does not send game actions. The primary WASD skill bar reference is eight keys; five additional logical labels are optional and native availability is unverified.

Device probe now optionally validates actual diagnostic axis samples, distinct XInput indices and neutral recovery, one device at a time with games closed. See [reference study](docs/REFERENCE_STUDY.md).