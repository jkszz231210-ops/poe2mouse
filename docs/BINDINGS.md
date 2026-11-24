# PoE2Mouse binding model (v0.1.4)
PoE2 keyboard/mouse references describe 13 bound skills (main/secondary bars); WASD and click-to-move layouts differ. Actual slot count, native remapping restrictions, controller-specific commands and hot-switch behavior must be recorded per game version and device mode from in-game Options > Input. The software cannot create more native game slots. Lab slot1..slot13 are logical labels, not verified game output. References: https://www.keyboardista.com/en/shortcuts/path-of-exile-2-game/ and https://frondtech.com/path-of-exile-2-controls-and-key-bindings/

## Schema 3
Each line: decimal Windows virtual key, target (p1/p2/both), action, optional modifier mask. Mask 1 Shift, 2 Ctrl, 4 Alt; sum for combinations. Exact chord overrides plain binding; if no exact chord exists, plain binding applies. The action selected at key-down stays selected until release, even if modifiers change. Solo movement modifiers affect shared movement only. No automatic action sequences. No external input output.
 
Example:
```
schema 3
82 p1 slot1
82 p2 slot13 1
70 p1 slot13 2
5 p2 flask1
```
R = P1 slot1; Shift+R = P2 slot13; Ctrl+F = P1 slot13; mouse side button 1 = P2 flask1. Key codes: A-Z 65-90, 0-9 48-57, mouse left/right/middle 1/2/4, sides 5/6, Shift/Ctrl/Alt 16/17/18, Space 32, F1-F12 112-123. OS-reserved chords may never reach the window; Alt menu interactions require testing. No wheel event binding yet.
 
Profile > Edit bindings opens a plain native text editor. Apply validates and saves atomically; Cancel discards changes. Duplicate key+mask rejected with line number. Old schema 1/2 profiles are loaded without rewriting; explicit save/export writes schema 3. Keep existing personal lab.profile when upgrading.
