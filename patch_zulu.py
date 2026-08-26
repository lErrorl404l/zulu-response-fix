#!/usr/bin/env python3
"""Patch Zulu Response's Zulu.u to talk to a local server instead of its dead backend.

The 2016 notify server (110.232.115.186:80) is dead and the game quits when it
fails the access check. This rewrites the hardcoded server address to loopback
(127.0.0.1) and the port to 8080, where a tiny local server answers correctly.

Cross-platform: the game files are identical on Windows and Linux/Proton.
Backs up the original to Zulu.u.bak. Reversible (restore the .bak, or let
Steam verify re-download the file).

Usage:  python3 patch_zulu.py [path/to/Zulu.u]
"""

import shutil
import sys
from pathlib import Path

IP_OLD = b"110.232.115.186"
IP_NEW = b"127.000.000.001"  # 16 chars; octal form -> 127.0.0.1 (Windows + Wine)
PORT_OLD = (80).to_bytes(4, "little")
PORT_NEW = (8080).to_bytes(4, "little")
PORT_OFFSET = 40  # bytes after each IP string where the port int sits


def patch(target: Path) -> int:
    data = target.read_bytes()
    if IP_NEW in data:
        print(f"{target}: already patched")
        return 0
    bak = target.with_suffix(target.suffix + ".bak")
    if not bak.exists():
        shutil.copy2(target, bak)
        print(f"backup: {bak}")
    count = 0
    pos = 0
    while True:
        i = data.find(IP_OLD, pos)
        if i < 0:
            break
        assert data[i + PORT_OFFSET : i + PORT_OFFSET + 4] == PORT_OLD, (
            f"unexpected bytes at offset {i}+{PORT_OFFSET}: "
            f"{data[i + PORT_OFFSET : i + PORT_OFFSET + 4].hex()}"
        )
        data = data[:i] + IP_NEW + data[i + len(IP_OLD) :]
        data = data[: i + PORT_OFFSET] + PORT_NEW + data[i + PORT_OFFSET + 4 :]
        count += 1
        pos = i + len(IP_OLD)
    target.write_bytes(data)
    print(f"{target}: patched {count} connection(s)")
    return count


if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "UDKGame/CookedPC/Zulu.u"
    patch(Path(arg))
