# Architecture
Pure input core -> local lab view. The core uses bounded arrays and no allocations in the event path. The parser runs outside that path. Windows messages are processed only in the lab window; no polling loop or background rendering. A later device backend and game adapter must be isolated from the core. The local sandbox supports simultaneous and independent logical action state for validation only.
 
Next milestones: profile editor with versioned atomic storage and bounded backups; split device identifiers from logical keys; focus/activation integration tests; input latency benchmark; native game-key discovery by read-only import; game-version compatibility matrix and manual verification on target hardware. Keep unsupported capabilities disabled.
 
Performance targets (not measurements): <=0.1% idle CPU, <=0.5% active CPU, <=30 MB working set, <=1 ms internal P99 latency, no background GPU rendering or idle disk writes. Final benchmarks on Ryzen 7 7735H / RTX 4050 Laptop / 16 GB class system, not the development workstation.
