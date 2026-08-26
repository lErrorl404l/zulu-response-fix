# Changelog

All notable changes are recorded here. Releases are signed tags on `main`.

## [1.0.1] - 2026-08-27

Fixed:
- notify responses are now matched case-insensitively. The game requests
  `zuNotify.php` with a capital N, which the earlier build missed.

Added:
- behavioural test harness (`test_harness.c`) that mimics the game's server
  calls without the game
- CI: build and test on Linux (Wine) and Windows (native), CodeQL code
  scanning, ClamAV malware scan of the built DLL, gitleaks secret scan
- security policy

## [1.0.0] - 2026-08-27

Initial release: the `dinput8` proxy DLL that revives the game's dead 2016
backend in-process.