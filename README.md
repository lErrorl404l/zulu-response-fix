# Zulu Response Fix

**Your game closes by itself after loading.** This fix stops that.

Zulu Response (2016) depended on an online server that no longer exists.
The game checks that server, cannot reach it, and quits. This one-file fix
runs the missing server locally inside the game. Nothing is removed from
the game, and no protection is bypassed. You still need your own copy of
the game.

## Install (one minute, no technical knowledge needed)

1. Go to **[Releases](https://github.com/lErrorl404l/zulu-response-fix/releases)**
2. Download the latest **Windows zip**
3. Unzip it anywhere
4. Double-click **`install.cmd`**
5. Launch Zulu Response from Steam

The installer finds the game folder by itself, on any Steam library drive.
No folders to dig through, nothing to configure.

**If Windows says "Unknown publisher"** when you run the installer:
click **More info**, then **Run anyway**. This is normal for a small
community mod without a paid signing certificate. The code is open source,
built and tested automatically on every change.

## Uninstall

Double-click **`uninstall.cmd`**. Or delete the file `dinput8.dll` from
the game's `Binaries\Win32` folder. The game returns to normal.

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