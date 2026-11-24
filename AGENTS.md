# Engineering contract
- Product goal: a single physical keyboard and mouse for local two-character control; physical controllers are optional. Preserve native game input where supported.
- Windows x64, C++20, CMake. Keep core independent of OS and game version. First milestone is a self-contained local sandbox, not an online game adapter.
- No game memory access, process injection, anti-cheat bypass, remote input or background key logging. Do not connect sandbox broadcast to an online game without separate permission and compatibility review.
- All user bindings (including mode/pause keys in future adapters) belong in versioned profiles, not compiled control logic.
- Development and final gaming/performance environments differ. Final verification targets a Ryzen 7 7735H / RTX 4050 Laptop / 16 GB class machine.
- Default offline. No telemetry, cache or logs unless explicitly needed; no persistent input history. Keep generated files out of Git. Use bounded backups and atomic replacement when implementing writable profiles.
- Never claim untested game compatibility. Record application version, adapter version, schema version and test evidence separately.
- Use focused commits, preserve existing behavior, build and run CTest, document unverified cases. Do not merge main without review.
