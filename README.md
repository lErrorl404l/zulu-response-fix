# Zulu Response — dead backend fix

Zulu Response (Steam appid 401250) exits at the menu because its 2016 notify
server (`110.232.115.186:80`) is dead and returns 404 HTML. This DLL revives
the backend in-process. It needs a legitimately owned copy of the game.

## Install

Copy `dinput8.dll` (from Releases) into:

```
<game folder>\Binaries\Win32\
```

Launch the game. Delete the file to uninstall.

## Build

```
i686-w64-mingw32-gcc -shared -O2 -o dinput8.dll \
    zulu_dinput8.c zulu_dinput8.def -lws2_32
```

## How it works

The game imports `dinput8.dll`, so a copy in the app folder loads before the
system one. The DLL forwards dinput8, redirects the game's `connect()` calls
for `110.232.115.186:80` to `127.0.0.1:8080`, and answers the game's access
check (`yes`) from an in-process HTTP server.

Tested on Linux/Proton. Windows build untested.

MIT. Not affiliated with Web Interactive Solutions.