#!/usr/bin/env python3
"""Fake backend for Zulu Response (Steam appid 401250).

The game's 2016 notify server (110.232.115.186:80) is dead and now returns
cPanel 404 HTML. The game parses the response, fails to see its expected
"yes", and quits. This server answers with the old protocol.

Expected responses (from the game's own 2017 logs):
  access check -> "yes"
  notify       -> "To Host Games your router must Port Forward ports :
                   6500, 7777 to 7790, 13000, 27900 *$*"

Route the game here with (root):
  iptables -t nat -A OUTPUT -d 110.232.115.186 -p tcp --dport 80 \
           -j REDIRECT --to-ports 8080
Remove with -D instead of -A.
"""

from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = 8080
NOTIFY = (
    "To Host Games your router must Port Forward ports : "
    "6500, 7777 to 7790, 13000, 27900 *$*"
)


class Handler(BaseHTTPRequestHandler):
    def _reply(self):
        length = int(self.headers.get("Content-Length", 0) or 0)
        body = self.rfile.read(length) if length else b""
        print(f"{self.command} {self.path} {body[:200]!r}", flush=True)
        self.send_response(200)
        self.send_header("Connection", "close")
        self.end_headers()
        if "notify" in self.path.lower():
            self.wfile.write(NOTIFY.encode())
        else:
            self.wfile.write(b"yes")

    do_GET = _reply
    do_POST = _reply

    def log_message(self, format, *args):
        pass


if __name__ == "__main__":
    print(f"listening on 127.0.0.1:{PORT}", flush=True)
    HTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
