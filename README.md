# PoE2Mouse

Windows-native keyboard and mouse input management research for local co-op. Project goal: preserve keyboard/mouse ergonomics, configurable controls and portable profiles, with independent game compatibility adapters.

## Status

Early development. No verified PoE2 integration or production-ready executable. The first implementation targets a self-contained local test application; game-facing behavior requires separate compatibility validation.

## Engineering priorities

C++20, CMake, event-driven input, no background rendering or network service, no game memory access or injection. Configuration is versioned and portable. Game versions, application releases and profile schema versions are independent. Changes must include regression tests and accurately report verification status.
