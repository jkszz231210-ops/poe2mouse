# PoE2Mouse binding model (v0.1.5)
User-verified primary WASD skill bar: eight inputs, left/middle/right mouse plus Q/E/R/T/F. A published WASD layout matches this observation: https://gamerch.com/poe2/889423 (click-to-move uses Q/W/E/R/T instead). Prior references describe an additional five secondary bindings, but availability and activation in the user's current client are unverified. Treat slot1..slot8 as the primary reference and slot9..slot13 as optional logical lab labels, not confirmed native capability. Actual native bindings must be checked in Options > Input for each mode/version. The application cannot add native slots.

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
