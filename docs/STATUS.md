# Capability status (0.1.1)
| Capability | Status |
|---|---|
| C++20 core, local P1/P2 shared and independent action state | implemented |
| Schema 1 profile loading, validation, serialization | implemented |
| Windows foreground keyboard/mouse diagnostic | implemented |
| Local movement simulation, wall and speed divergence | implemented |
| Profile import/export, reload and persistence | implemented; interactive Windows test pending |
| Portable Windows package workflow | configured; latest build pending |
| GUI key capture/rebinding, schema migration | planned |
| Raw Input, device identity, physical controller | planned |
| Game-facing adapter, PoE2 native key import and hot switching | not implemented or verified |
| Later game version compatibility | architecture goal |
| Performance / 72-hour tests | not measured |
Prior baseline commit 9c6b58a passed both Windows and Ubuntu build/CTest: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45322230981
The new revision requires its own CI and manual verification. No game output, hooks, virtual driver or telemetry.
