# Hybrid controller MVP / 0.1.18

## Scope
- One physical Xbox/XInput-compatible controller is read through XInput.
- The app creates two ViGEm Xbox 360 virtual controllers as before.
- Physical left stick is mirrored to both virtual controllers.
- Physical right stick, D-pad, face buttons, shoulders, triggers, Start and thumb clicks are routed to the active role only.
- Physical Back/View toggles the active role (P1/P2) and is consumed. The button mask is configurable in `bridge.options`.
- Ordinary keyboard/mouse bindings whose profile target is P1 follow the active role. Explicit P2 modifier-layer bindings remain fixed to P2.
- F10 pauses and neutralizes both virtual controllers. Focus loss and physical controller disconnect also pause and neutralize.
- The role toggle defaults to XInput Back (decimal 32); set `controller_role_toggle` to another single standard XInput button mask in schema 2 options.
- `keyboard_follow_active false` disables keyboard role following.

## Important device visibility requirement
The physical controller must remain visible to PoE2Mouse but hidden from the game, otherwise the game may enumerate three controllers (physical + two virtual). Use HidHide only after reading its official documentation:
https://docs.nefarius.at/projects/HidHide/
Whitelist this exact executable in HidHide: `input_bridge.exe`. Hide only the physical controller device. Do not whitelist the game or Steam if the intent is for them not to see the physical device. Do not hide either ViGEm virtual device. Reconnect the controller after applying HidHide changes. This build does not install, configure, or toggle HidHide automatically. A driver install may require administrator consent and reboot.

## Quick test
1. Close games and other virtual controller tools.
2. Connect exactly one XInput-compatible physical controller before clicking "连接手柄 + 双虚拟设备".
3. Complete "设备输出自检".
4. Click "启用混合输入测试" to arm the local window.
5. Move the physical left stick: both virtual controllers should show the same left-stick direction.
6. Press Back once: the current role label changes P1/P2. Back is not forwarded.
7. Press A/B/X/Y or a trigger: only the selected virtual controller should change.
8. Press W or Q: keyboard input follows the selected role. Existing explicit P2 modifier-layer bindings remain P2.
9. F10 pauses; release all controls before re-arming.

## Supported controller
This MVP reads XInput via XInput 9.1.0. Native DirectInput-only or HID-only controllers are not yet supported. A controller must already be exposed as XInput by its driver or an explicitly configured remapper. Close other virtual gamepad emulators to avoid ambiguous enumeration.

## Limits
- Windows build only. CI verifies compilation and pure routing tests; physical controller runtime still requires the user's machine.
- This is a generic virtual-controller remapper, not a PoE2 game adapter. No process injection, memory reading, anti-cheat bypass, screen recognition, automated target selection or background input logging.
- It does not verify game-side player assignment, local co-op, inventory/trade behavior or mixed-input hot switching.
- The shared left-stick output intentionally sends one manually held axis state to both virtual controllers. GGG's Terms of Use prohibit automated software or bots; the terms do not specifically clarify this exact manual local co-op remapping pattern. Do not assume GGG has approved it. Review the current terms and obtain clarification if needed before using it online.
