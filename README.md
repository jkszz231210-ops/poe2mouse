# PoE2Mouse
Windows-native keyboard and mouse input management research for local co-op. A single physical keyboard/mouse is the primary input; physical controllers are optional. Native game input is preferred when supported.

## v0.1 development baseline
A self-contained local input lab and portable C++20 state engine. **Not a working PoE2 adapter.** The lab only displays logical P1/P2 action states in its own window; it does not send inputs to other applications.

## Build
Install CMake 3.22+ and a C++20 compiler. On Windows use Visual Studio 2022 with Desktop development with C++.
```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
Open build/Release/input_lab.exe (or build/input_lab.exe for single-config generators). The profile is copied next to the binary. Edit lab.profile to rebind every mapped action; restart the lab to reload. WASD and space are shared logical test actions; left/right mouse and Q/E test separate actions; arrow keys test P2 movement. The lab does not record key history. No third-party dependencies, network service or driver required for this milestone.

See [engineering contract](AGENTS.md), [architecture](docs/ARCHITECTURE.md) and [status](docs/STATUS.md). Game versions, app releases and profile schema are independent.
