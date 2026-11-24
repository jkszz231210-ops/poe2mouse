# Capability status
| Capability | Status |
|---|---|
| C++20 core, explicit P1/P2 local state | implemented; Linux GCC 14.2 core checks passed |
| Schema 1 profile loading and validation | implemented; Linux GCC 14.2 core checks passed |
| Windows foreground keyboard/mouse diagnostic | implemented; Windows build and runtime pending |
| Config writer, GUI rebind/import/export, migrations | planned |
| Raw Input, device identity, physical controller | planned |
| Game-facing adapter, PoE2 native key import, 1.0 hot switching | unverified / not implemented |
| 1.1+ compatibility | architectural goal; not verified |
| Performance, 72-hour run | not measured |
The simulator has no OS input synthesis, hooks, game connection or virtual driver. It only displays its own input state. No release binary yet.

Verification: GCC 14.2, C++20, -O2 -Wall -Wextra -Wpedantic -Werror; compiled and ran core_tests successfully in Linux. The local copies of the header and test source were verified against Git blob IDs c092dc137a23dd57f77d8396cd20bf1a429c82e6 and c772ac0f6b50c9815d9148925f8ac71351d7c76e. Windows and CI verification remain pending.
