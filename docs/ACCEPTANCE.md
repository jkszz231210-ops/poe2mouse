# Release acceptance and open questions / 0.1.11
Do not claim a playable release until the four P0 gates are verified on the user's home Windows laptop. Record actual client version, region, input mode, observations and commit. Do not require cosmetic polish.
 
## P0 core
- [ ] Two virtual controllers are accepted by the native local co-op UI. Confirm assignment and reconnection. Single account / shared scene.
- [ ] Independent keyboard/mouse remapping does not conflict with native UI/input switching. No suppression, injection or evasion in current bridge.
- [ ] Native gamepad auto-targeting with neutral right stick: test normal clear, melee, ranged, boss, ground skill, corpse/ally skills. Per-skill targeting mode is native game functionality (0.2.0 patch notes). Prefer this over simulating precise pointer aim. Optional explicit direction override only if needed. No screen tracking or automated target selection by this application.
- [ ] Record actual controller skill pages, buttons and native modifier actions per player; primary keyboard reference eight keys. Map the game-confirmed controls, not guessed slot labels. No game broadcast.
 
## P1 operational details (record now, resolve after core; minimum safety cannot wait)
- [ ] Pause keeps devices neutral; detach releases both devices without exiting application. Reconnect explicitly; game-side return to solo and rejoin workflow may require character screen or relog, must verify.
- [ ] Normal exit, forced Task Manager termination, OS sleep, input unplug, driver loss, game disconnect, crash and restart. Confirm no stuck input or extra virtual devices; never auto resume. Game may pause or disconnect when controller disappears depending on settings/version. Restore original single-player input via game UI.
- [ ] Market / trade: before transaction disable output; test market browse/search, buy/sell, stash/inventory, chat, town, external browser, P2 permissions and native input mode. Do not automate purchases or chat. No assertion of unaffected trade or safe account status before testing. 
- [ ] Report bug through chat or issue with version/commit, error code, expected/actual, reproduction and device slots. Bridge console and opt-in bounded clipboard report available in code; crash-safe operational logs remain pending. No raw keystroke history or automatic upload.
- [ ] Config persistence / migration / rollback, mouse macro buttons only as manual inputs, direct key capture, profiles, mode keys, conflicts.
- [ ] Performance: home Ryzen 7 7735H / RTX 4050 Laptop / 16 GB, repeat same scene/settings with and without software, measure CPU, working set, FPS and 1% low, input latency P99 and 72h stability. Goals idle <=0.1% CPU, active <=0.5%, <=30 MB working set, software P99 <=1 ms, avg FPS loss <=1%, 1% low loss <=2%; targets not measurements. Paused bridge should unsubscribe Raw Input, no periodic monitor, no disk log/network/GPU drawing loop. Verify.
- [ ] Single instance and emergency shortcut configurable; error recovery, driver uninstall instructions, no stale files.
 
## Evidence
- Gamepad directional override of native auto-targeting: https://wccftech.com/path-of-exile-2-launches-this-year-in-early-access-on-pc-and-consoles-with-couch-co-op-cross-play-progression/
- Per-skill targeting mode introduced in 0.2.0: https://poe2db.tw/us/Version_0.2.0
- Historical controller disconnect pause behavior: https://poe2db.tw/sp/markdown_diff?diff=1428&oldid=0
- Client-specific market and single-player transition are unknown until tested.

Track discovered defects in ISSUE_LOG.md, separate CI evidence from actual device/game behavior.

## First-use gate
- [ ] Launch without DLL/driver, view Chinese tutorial and dependency instructions, inspect real binding list, recover invalid configuration, copy report. Verify no device creation before explicit connect and no accidental output. No visual polish required.

- [ ] Office test: no game/driver; actionable guide, same-profile preview, Chinese edit/save/reject conflict, reserved native pause. Game UI compatibility matrix: NATIVE_UI.md.

- [ ] After explicit device self-check, main-window real input test: hold P1 movement, P2 movement, independent button/chord, release, F10 pause, re-arm; observe actual XInput, not simulated state. Persistent mismatches pause and invalidate self-check. Short taps require separate timing tests.

- [ ] Dependency setup: existing DLL and service retained; clean installation downloads pinned artifacts, rejects hash/signature mismatch, extracts x64 DLL only, driver installer requires explicit consent, cleans own temp, detects policy/network errors, recheck succeeds. No game required; use approved test machine only.
