# Public reference study / v0.1.6
Scope: study public documentation and interfaces, independent implementation. No proprietary code extraction, no anti-cheat evasion.
 
| Reference | Useful concept | Project decision |
|---|---|---|
| reWASD Advanced Mouse and output device settings | mouse-to-stick curves, minimum deflection, sensitivity | implement pure radial curve calibration; game-specific aiming remains unverified |
| Steam Input Mode Shifting / Action Set Layers | base mappings, temporary overrides | retain schema 3 exact chord and latched release, future explicit UI/action layers |
| JoyShockMapper | configurable analog modes and backend isolation | separate pure calibration and device backend; no dependence on its executable |
| ViGEmClient public header | assigned XInput index, neutral reset | query actual index, verify sample and neutral through XInput, report failures |
 
Links:
- https://www.help.rewasd.com/basic-functions/advanced-mouse-settings.html
- https://www.help.rewasd.com/basic-functions/virtual-controller-settings.html
- https://partner.steamgames.com/doc/features/steam_controller/mode_shifting
- https://partner.steamgames.com/doc/features/steam_controller/action_set_layers
- https://github.com/JibbSmart/JoyShockMapper
- https://github.com/nefarius/ViGEmClient/blob/master/include/ViGEm/Client.h
 
Important distinction: a physical mouse supplies relative motion or cursor coordinates; a gamepad stick supplies deflection. They are not equivalent. Pure radial curve handles normalized deflection only; it does not convert mouse motion into a reliable PoE2 target. Verify native right-stick targeting and hot switching before selecting any game adapter. Historical PC couch co-op guides require two controllers; current client support remains unverified. No game-facing output adapter in this build.
 
Priority: actual device lifecycle/readback, one-to-one supported remapping and native compatibility, then ergonomics, UI polish last. No game broadcast is enabled. Avoid third-party background services and default disk logging.
