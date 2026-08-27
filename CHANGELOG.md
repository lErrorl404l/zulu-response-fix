# Changelog

All notable changes are recorded here. Releases are signed tags on `main`.

## [1.0.2] - unreleased

Changed:
- installation is now drag-and-drop only: one file into
  `Binaries\Win32`. The `.cmd` installer scripts were removed: for a
  single-file mod they added suspicion without value, and they were not
  cross-platform.
- CI jobs split: `build-linux`, `build-windows`, `scan`
- releases are built entirely in CI: a tagged push builds the DLL,
  generates an SPDX SBOM (Syft) and attaches zip + SBOM to the release
- README gained a "Is it safe?" section (hash, provenance, scans)

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