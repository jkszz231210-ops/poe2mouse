# Engineering contract
- Product goal: a single physical keyboard and mouse for local two-character control; physical controllers are optional. Preserve native game input where supported.
- Windows x64, C++20, CMake. Keep core independent of OS and game version. First milestone is a self-contained local sandbox, not an online game adapter.
- No game memory access, process injection, anti-cheat bypass, remote input or background key logging. Do not connect sandbox broadcast to an online game without separate permission and compatibility review.
- All user bindings (including mode/pause keys in future adapters) belong in versioned profiles, not compiled control logic.
- Development and final gaming/performance environments differ. Final verification targets a Ryzen 7 7735H / RTX 4050 Laptop / 16 GB class machine.
- Default offline. No telemetry, cache or logs unless explicitly needed; no persistent input history. Keep generated files out of Git. Use bounded backups and atomic replacement when implementing writable profiles.
- Never claim untested game compatibility. Record application version, adapter version, schema version and test evidence separately.
- Use focused commits, preserve existing behavior, build and run CTest, document unverified cases. Do not merge main without review.

## Release priorities
1. Functionality and reproducible usage: get the smallest real feature working, explicitly state limitations, supply a concise Chinese quick-start and recovery instructions.
2. Feedback and observability are release gates from the first testable build: stable error identifiers, privacy-safe bounded diagnostics, minimal reproduction instructions, and regression tests for confirmed defects. No automatic upload or raw keystroke logging. Diagnostics should be exportable without installing development tools.
3. After repeated real usage, prioritize ergonomics and counterintuitive bindings, configuration migration and stability. Cosmetic polish follows functional validation.

- Model native game action slots separately from physical bindings. Do not invent extra game slots or assume keyboard and controller layouts are identical. Preserve the ability to bind independent role actions and user-selectable modifiers. Confirm actual slot mappings per game version and input mode.

- Acceptance gates and deferred operational details live in docs/ACCEPTANCE.md. Prefer native controller auto-targeting and per-skill target modes over recreating mouse aim; no automated target selection. Pause and detach are distinct. No gameplay release claims before the P0 gates are verified.

- Record newly discovered defects and their fix/test evidence in docs/ISSUE_LOG.md or GitHub Issues; distinguish static review, CI and runtime observations.

- Usability is a release gate independent of cosmetic design: main executable must open help even when dependencies/configuration are missing, provide Chinese first-use guidance, explain pause versus detach, and expose actual binding and diagnostic information. No silent driver installation.

- First-use guide should perform real steps, not just prose. The main bridge's driver-free preview and editor must share the exact active configuration; apply validated edits without restart, neutralize output before changes. Native inventory/map/shop/chat shortcuts must be considered separately from combat; explicit pause does not imply game-side input-mode support.

- Dependency preparation must be opt-in, pinned URL and SHA-256, driver installer signature checked, no silent elevation or execution-policy bypass, no overwriting existing user DLL, no cleanup outside task-owned paths. Manual offline path remains available.
