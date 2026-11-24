# Capability status (0.1.14)
| Capability | Status |
|---|---|
| C++20 core, local P1/P2 shared and independent action state | implemented |
| Schema 1 profile loading, validation, serialization | implemented |
| Windows foreground keyboard/mouse diagnostic | implemented |
| Local movement simulation, wall and speed divergence | implemented |
| Profile import/export, reload and persistence | implemented; interactive Windows test pending |
| Portable Windows package workflow | 0.1.10 Windows/Linux CI passed; 0.1.14 CI pending |
| Bounded diagnostics, explicit clipboard/file export, Chinese feedback guide | implemented; CI and Windows interactive validation pending |
| Schema 1/2 compatibility, movement-only solo modifiers, upgrade-safe defaults | implemented; latest CI pending |
| Read-only Raw Input enumeration and XInput slot snapshot | implemented; Windows manual verification pending |
| Native text binding editor, schema 3 chords, 8 primary + 5 optional logical skill labels per player | implemented; CI and manual validation pending |
| Neutral virtual controller lifecycle probe | implemented; requires explicit external driver/DLL and Windows device verification |
| Explicit per-device analog sample and XInput readback | implemented; real driver test pending |
| Pure radial curve calibration | implemented; CI pending; no game integration |
| Graphical key capture, wheel binding, actual game slot mapping | planned |
| Independent Raw Input to virtual controller report bridge | implemented; CI and physical device test pending |
| Explicit attach/detach without exit, pause unsubscribes input | implemented; Windows runtime pending |
| Physical key identity and logical hold aggregation | implemented; CI and runtime verification pending |
| Manual complete controller controls, bridge diagnostic copy | implemented; CI and runtime verification pending |
| Chinese bridge UI, embedded tutorial, lazy SDK load, current binding view and configuration reload | implemented; Windows runtime pending |
| Native keyboard conflict and actual game targeting | pending |
| Game-facing adapter, PoE2 native key import and hot switching | not implemented or verified |
| Later game version compatibility | architecture goal |
| Performance / 72-hour tests | not measured |
Prior baseline commit 9c6b58a passed both Windows and Ubuntu build/CTest: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45322230981
The new revision requires its own CI and manual verification. No verified game adapter or telemetry. Independent bridge uses foreground event notification, not a keyboard hook. Optional standalone neutral-device probe requires external driver.

Previous v0.1.1 commit 57d984d passed CI and generated a Windows x64 artifact: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45322768378 . No game output is implemented.

Previous v0.1.2 commit 77876df passed Windows/Linux CI: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45323587594 . No game-facing output in that prior revision.

Prior 0.1.5 commit fdc3012 passed both CI platforms: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45330149185 .

0.1.6 CI passed: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45330941590 . 0.1.14 adds an explicitly armed independent remapper; no broadcast output.

Acceptance gates: docs/ACCEPTANCE.md. Native auto-target first; no precise mouse aiming requirement unless gameplay tests show a need.

0.1.8 CI passed: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45338189598 . Issue ledger: docs/ISSUE_LOG.md.

0.1.9 CI passed: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45339921332 .

0.1.14: interactive guide, same-config offline preview, localized binding editor, reserved native pause; Windows runtime and native UI compatibility pending. The older independent lab still uses its own profile.

Office 0.1.11 runtime observation: driver connection, target add/update, index API and removal succeeded, distinct-slot/neutral validation failed. PM-018 diagnostic improvements in 0.1.14, not yet verified. Game integration remains untested.

Office evidence: 0.1.11 joy.cpl shows two controllers; XInput slots 0/1 connected and all absent after cleanup. E206 prevents actual sample readback. 0.1.14 uses sequential XInput discovery with mandatory explicit readback gate; runtime retest pending.

0.1.13 Windows/Linux CI success: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45350595952 . 0.1.14 adds current-state monitor only during explicit local test; not a gameplay polling loop. Windows runtime pending.

0.1.14 Windows/Linux CI passed https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45351492586 . 0.1.15 adds opt-in dependency preparation script and UI entry. Network download/install runtime pending; no automatic install or policy bypass.

0.1.15 office: E206 resolved (observed XInput slots 0/1, SDK both 0); E207 exact neutral failed on both with identical axis offsets. PM-022 adds explicit opt-in setup initialization and strict exact-neutral readback, pending CI/runtime. No game compatibility claims.

## Latest verified office milestone: v0.1.16 (7c86d44), 2026-11-24
User-provided real Windows console: P1/P2 observed slots 0/1, SDK both 0 (W206), P1 and P2 initialization PASS, Both devices exact neutral PASS, Assigned slots 0 and 1, Device 1: sample=PASS neutral=PASS, Device 2: sample=PASS neutral=PASS. The independent sample test checks other device neutral. PM-019 and PM-022 verified fixed for this environment. CI: https://github.com/jkszz231210-ops/poe2mouse/actions/runs/45356904204 . Manual Raw Input to XInput live test, pause/re-arm, game compatibility and performance are still pending; earlier E302 occurred before self-check, not a post-check failure. No new executable needed for next test.

Office live input milestone (user-reported v0.1.16, 2026-11-24): all nine manual keyboard/mouse tests passed, including independently held W+Up, mouse/Shift+mouse, full neutral on release and F10 pause. Core physical input to actual dual XInput output verified on office PC. Subjective responsiveness good; no latency benchmark. Compact keyboard requires user-defined Ctrl+WASD P2 chords (not shipped by default). Re-arm, crash recovery, actual game and home performance remain unverified.

0.1.17: user reports repeated external selection E302 in v0.1.16 (PM-025). Add explicit preflight, bounded release wait, pinned foreground window and per-cause diagnostics; no change to verified device backend. CI and external-window runtime pending.

Home v0.1.17 runtime evidence (user console, 2026-11-24): distinct slots and both neutral initialization/readback PASS; pre-check E302-C correctly blocks arming; after self-check local arm, external arm, then local re-arm all PASS. Target application unspecified; actual external input reception, game controls and performance remain open. No code changes for this milestone.


## v0.1.18 hybrid controller MVP
- Implemented pure hybrid routing core: physical left stick shared to P1/P2, right stick/buttons/triggers/D-pad to active role, configurable Back/View role toggle, radial deadzones, saturating merge with keyboard state.
- Keyboard P1 bindings follow active role by default; explicit P2 modifier-layer bindings remain fixed P2. Schema 1 options remain compatible; schema 2 adds controller toggle and follow-active flag.
- Windows bridge reads exactly one pre-existing XInput controller before creating virtual pads; timer-driven XInput polling only while explicitly armed. Disconnect/focus loss/F10 neutralize both virtual outputs.
- HidHide is a manual prerequisite for game testing so the game sees only two virtual controllers; no driver install or device hiding changes are automated.
- CI pending; XInput hardware test, HidHide setup, PoE2 player assignment, game rules and performance unverified.
