# Capability status
| Capability | Status |
|---|---|
| C++20 core, explicit P1/P2 local state | implemented; test execution pending |
| Schema 1 profile loading and validation | implemented; test execution pending |
| Windows foreground keyboard/mouse diagnostic | implemented; Windows build and runtime pending |
| Config writer, GUI rebind/import/export, migrations | planned |
| Raw Input, device identity, physical controller | planned |
| Game-facing adapter, PoE2 native key import, 1.0 hot switching | unverified / not implemented |
| 1.1+ compatibility | architectural goal; not verified |
| Performance, 72-hour run | not measured |
The simulator has no OS input synthesis, hooks, game connection or virtual driver. It only displays its own input state. No release binary yet.
