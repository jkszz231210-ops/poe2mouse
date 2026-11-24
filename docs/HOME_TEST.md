# Home test: v0.1.6 local lab
This package does not connect to PoE2, install drivers, send external inputs or need a compiler. Extract all files together into a writable folder on D:. Start input_lab.exe. Windows interactive testing is still required.
 
1. Hold WASD: both colored markers move, at intentionally different speeds. Observe divergence and the obstacle.
2. Arrow keys independently correct P2. Left/right mouse buttons activate independent attack indicators. Q/E activate independent skill indicators. Space activates the shared dodge indicator in the lab.
3. Hold a movement key and click/release the mouse. Movement must continue. Switch windows while holding a key: all actions must clear. Release and return: no movement until new input.
4. Export a profile through Profile menu. Edit the exported text (decimal Windows VK code, target p1/p2/both, action up/down/left/right/attack/skill/dodge). Import it. Restart: imported configuration must persist. Import malformed or duplicate bindings: previous config must be retained. Revert by importing original profile.
5. Close the app. No background process, logs, driver or cache should remain. Check task manager idle CPU/working set; report observations, not certified performance.
 
## Separate PoE2 native compatibility observations (manual, no lab connection)
Use existing characters and the game's supported couch co-op menu. Record game version, region and client type. Check whether a physical keyboard/mouse can control P1 while your existing physical controller is assigned to P2; check native input hot switching, P1/P2 independence, inventory interaction and whether reconnect changes assignment. Record only observations. No game-facing adapter is present in this package.

6. Help > Copy diagnostic report: paste into Notepad and verify app version, event counts and lack of raw input or paths. Save diagnostics.txt twice; confirm only one report file exists. See FEEDBACK.md for Chinese instructions.

7. With the new default profile, hold W and then Shift: P1 only. Release Shift: both. Hold Ctrl: P2 only. Hold both: shared movement stops, but explicit P2 arrow movement and individual attack keys still work. Release all: neutral. Existing schema 1 profiles remain unchanged; import lab.default.profile deliberately to try the new defaults.
8. Help > Check input devices: record raw keyboard/mouse/HID counts and XInput slots. Connect the physical controller and repeat; disconnect and repeat. The probe is read-only, not a game adapter, and does not create virtual devices. Counts may include composite or virtual devices.
9. Upgrade from 0.1.2 by extracting into the same directory after closing the app. New archives contain lab.default.profile, never lab.profile. Existing lab.profile takes precedence. Verify bindings survive restart. Back up manually before upgrade.

10. Profile > Edit bindings: add `82 p1 slot1` and `82 p2 slot13 1` to a profile without those entries, Apply, then R and Shift+R. Release Shift before R and check the selected action remains active until R releases. Restart to confirm persistence. Try duplicate entries and Cancel to verify prior configuration is unchanged. Use default profile for examples.

11. Optional device lifecycle probe: close games, follow DEVICE_PROBE.md, check two neutral devices are created and removed; do not confuse this with PoE2 integration.

12. With all games closed, opt into TEST in the device probe. Copy assigned indices and PASS/FAIL sample/neutral lines. Confirm both devices disappear on exit. A skipped test is not a pass.
