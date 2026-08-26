# Zulu Response — dead backend revival

Zulu Response (Steam appid 401250) is a 2016 Unreal Development Kit (UDK)
game by Web Interactive Solutions. Its online backend is dead. The game
hardcodes its notify server as `110.232.115.186:80`. That server no longer
serves the game and returns 404 error pages. The game's access check fails,
and the game exits at the menu.

This project revives the backend locally. It needs a legitimately owned copy
of the game. It does not bypass any protection.

## Install

Copy `dinput8.dll` into:

```
<game folder>\Binaries\Win32\
```

Launch the game. That is all.

To uninstall, delete the file.

## Build

The DLL is a 32-bit Windows proxy. Build it with mingw-w64:

```
i686-w64-mingw32-gcc -shared -O2 -o dinput8.dll \
    zulu_dinput8.c zulu_dinput8.def -lws2_32
```

## How it works

The game imports `dinput8.dll`, so Windows and Wine load a copy in the app
folder before the system one. The DLL then does three things:

1. **Forwards** `DirectInput8Create` and the other dinput8 exports to the
   real dinput8 (located via `GetSystemDirectoryA`). The game's input is
   untouched.
2. **Rewrites the game's import table** so calls to `connect()` for
   `110.232.115.186:80` target `127.0.0.1:8080` instead. WININET
   (`InternetConnectA/W`, `InternetOpenUrlA/W`) is covered the same way.
3. **Runs a minimal HTTP server** on `127.0.0.1:8080` that answers the
   game's protocol (see below).

The game cannot tell that its server is now local.

## The protocol

The server responses were recovered from the game's own 2017 launch logs:

| Request | Expected response |
|---|---|
| Access check | `yes` |
| Notify | `To Host Games your router must Port Forward ports : 6500, 7777 to 7790, 13000, 27900 *$*` |

The dead server returns 404 HTML. The game parses the garbage, does not see
`yes`, and quits. The local server returns the correct text.

## Testing status

- **Linux/Proton: verified.** The game passes the access check, reaches the
  menu, and stays up. Runs fullscreen, no other changes required.
- **Windows: not yet tested.** The build and the hook targets are identical
  on Windows, but it needs a real test. Testers welcome.

## Logs

The DLL writes `zulu_fix.log` to the Windows temp folder (`%TEMP%`). On
Proton/Wine the file is under:

```
<prefix>\drive_c\users\<user>\AppData\Local\Temp\zulu_fix.log
```

The log records hook installation, each redirect, and each request served.

## Alternatives

- `zulu_backend.py` — a standalone Python server with the same protocol.
  Useful with a firewall redirect (`iptables -t nat -A OUTPUT -d
  110.232.115.186 -p tcp --dport 80 -j REDIRECT --to-ports 8080`) if you
  prefer no DLL.
- `patch_zulu.py` — patches the hardcoded server address in `Zulu.u`
  (the game's compiled script) directly.

## License

MIT. This project is not affiliated with Web Interactive Solutions.