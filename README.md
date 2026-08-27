# Zulu Response Fix

[![build and test](https://img.shields.io/github/actions/workflow/status/lErrorl404l/zulu-response-fix/build.yml?label=build%20%26%20test)](https://github.com/lErrorl404l/zulu-response-fix/actions/workflows/build.yml)
[![code scanning](https://img.shields.io/github/actions/workflow/status/lErrorl404l/zulu-response-fix/codeql.yml?label=code%20scanning)](https://github.com/lErrorl404l/zulu-response-fix/actions/workflows/codeql.yml)
[![release](https://img.shields.io/github/v/release/lErrorl404l/zulu-response-fix)](https://github.com/lErrorl404l/zulu-response-fix/releases)
[![license](https://img.shields.io/github/license/lErrorl404l/zulu-response-fix)](LICENSE)

**Your game closes by itself after loading.** This fix stops that.

Zulu Response (2016) depended on an online server that no longer exists.
The game checks that server, cannot reach it, and quits. This one-file fix
runs the missing server locally inside the game. Nothing is removed from
the game, and no protection is bypassed. You still need your own copy of
the game.

## The game

Zulu Response is a 2016 early-access game by Web Interactive Solutions,
built with the Unreal Development Kit. It is on
[Steam](https://store.steampowered.com/app/401250/). The developers'
[ModDB game page](https://www.moddb.com/games/zulu-response) and
[company page](https://www.moddb.com/company/web-interactive-solutions)
are still up; the original website is offline.

## Install (one minute, no technical knowledge needed)

1. Go to **[Releases](https://github.com/lErrorl404l/zulu-response-fix/releases)**
2. Download the latest **Windows zip** (the file whose name ends in `-windows.zip`)
3. Unzip it anywhere
4. Find your game folder:
   - In Steam, right-click **Zulu Response** → **Manage** → **Browse local files**
5. In the folder that opens, go into **`Binaries`** → **`Win32`**
6. **Drag `dinput8.dll`** from the unzipped folder into **`Win32`**
7. Launch Zulu Response

That is the whole install: one file dragged into one folder, the same way
every game mod works. Nothing is installed system-wide, no script runs,
nothing needs administrator rights.

**Linux / Steam Deck (Proton):** the same file goes into the same folder
inside the game's Proton prefix:
`steamapps/common/Zulu Response/Binaries/Win32/`.

**If Windows says "Unknown publisher"** when you unzip: this is normal for
a small community mod without a paid signing certificate. The file is
built by automated CI from this exact source code, and you can verify it
(see below).

## Uninstall

Delete `dinput8.dll` from `Binaries\Win32`. The game returns to normal.

## Is it safe?

The fix is one small, open file that does one thing: redirect the game's
check of one dead server to a local copy of that server's 2017 response.
You can verify every claim below yourself:

- **Source**: the whole project is ~300 lines of C in this repository.
- **CI-built**: the released DLL is compiled by GitHub's servers from this
  source on every change, not on anyone's personal computer.
- **Hash**: each release publishes the SHA-256 of the DLL. Compare it with
  what you downloaded — if they match, your file is byte-for-byte the CI
  build.
- **Scans**: every release is uploaded to VirusTotal automatically; the
  analysis link appears in the release notes. CI also runs ClamAV on every
  build. The first release's VirusTotal result is
  [here](https://www.virustotal.com/gui/file/f7ee3cdf2f4db1898f2fb74442294dd65dce1333aceee356386c2ba98d5d0d04/).
- **SBOM**: an SPDX SBOM is generated in CI at every release (Syft) and
  attached to the release. There are no third-party runtime components.
- **Tests**: CI runs behavioural tests against the DLL on Windows and
  Linux. The tests check it only redirects the one dead server and answers
  the game's protocol correctly; all other network traffic passes through
  untouched.

## Does it work?

- Verified on **Windows** and **Linux** (Steam Proton).
- CI runs the same behavioural tests on both, automatically, on every
  change: the fix's redirects and responses are checked against the game's
  exact call patterns.
- The released DLL is scanned for malware by CI before every release.

## How it works (short version)

The game's access check talks to `110.232.115.186:80`. That server is dead
and answers with an error page, so the game quits. The fix makes the game
talk to a small local server inside the DLL instead, which answers the way
the original server did in 2017.

---

## For developers

### Build

```
i686-w64-mingw32-gcc -shared -O2 -o dinput8.dll \
    zulu_dinput8.c zulu_dinput8.def -lws2_32
```

### Test

`test_harness.c` mimics the game's server calls without the game: every
endpoint, method, and body the game sends, over both raw sockets and
WININET, plus a pass-through check.

```
i686-w64-mingw32-gcc -O2 -o test_harness.exe test_harness.c \
    -lws2_32 -ldinput8 -lwininet
wine test_harness.exe
```

All checks must pass (exit 0). CI runs the same steps on every push, on
Linux (Wine) and Windows (native), plus CodeQL and a ClamAV scan.

See `CONTRIBUTING.md` for the ground rules (signed commits, DCO).

## License

MIT. Not affiliated with Web Interactive Solutions.