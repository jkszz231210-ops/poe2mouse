# Capability status (0.1.4)
| Capability | Status |
|---|---|
| C++20 core, local P1/P2 shared and independent action state | implemented |
| Schema 1 profile loading, validation, serialization | implemented |
| Windows foreground keyboard/mouse diagnostic | implemented |
| Local movement simulation, wall and speed divergence | implemented |
| Profile import/export, reload and persistence | implemented; interactive Windows test pending |
| Portable Windows package workflow | 0.1.3 CI passed; 0.1.4 CI pending |
| Bounded diagnostics, explicit clipboard/file export, Chinese feedback guide | implemented; CI and Windows interactive validation pending |
| Schema 1/2 compatibility, movement-only solo modifiers, upgrade-safe defaults | implemented; latest CI pending |
| Read-only Raw Input enumeration and XInput slot snapshot | implemented; Windows manual verification pending |
| Native text binding editor, schema 3 chords, 13 logical skill slots per player | implemented; CI and manual validation pending |
| Graphical key capture, wheel binding, actual game slot mapping | planned |
| Raw Input event routing, device identity, physical controller input/output | planned |
| Game-facing adapter, PoE2 native key import and hot switching | not implemented or verified |
| Later game version compatibility | architecture goal |
| Performance / 72-hour tests | not measured |
Prior baseline commit 9c6b58a passed both Windows and Ubuntu build/CTest: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45322230981
The new revision requires its own CI and manual verification. No game output, hooks, virtual driver or telemetry.

Previous v0.1.1 commit 57d984d passed CI and generated a Windows x64 artifact: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45322768378 . No game output is implemented.

Previous v0.1.2 commit 77876df passed Windows/Linux CI: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45323587594 . No game-facing output in this revision.
