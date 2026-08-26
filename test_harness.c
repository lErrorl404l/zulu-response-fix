/*
 * test_harness.c - behavioural tests for the Zulu Response proxy DLL.
 *
 * Mimics the game's server calls so the fix can be verified without the
 * game. Loads dinput8.dll via the import table exactly like the game does,
 * then exercises every path the game uses:
 *
 *   raw sockets (the game's TcpLinkClient):
 *     GET  /zuGameInfo.php   -> "yes"
 *     POST /zuSecure.php     -> "yes"   (body: key=Zulu 1.06&submit=960327)
 *     GET  /zuNotify.php     -> port-forward message
 *     POST /zuNotify.php     -> port-forward message
 *   normal connects          -> pass through untouched
 *   wininet (the game's HttpRequest layer):
 *     InternetConnectA("110.232.115.186", 80)
 *     InternetOpenUrlA("http://110.232.115.186/...")
 *
 * Run with the DLL in the same directory as this exe.
 * Build: i686-w64-mingw32-gcc -O2 -o test_harness.exe test_harness.c \
 *            -lws2_32 -ldinput8 -lwininet
  * SPDX-License-Identifier: MIT
 */
#define DIRECTINPUT_VERSION 0x0800
#include <winsock2.h>
#include <windows.h>
#include <wininet.h>
#include <dinput.h>
#include <stdio.h>
#include <string.h>

static int fails;

/* mingw's dinput8 import library does not export the IID; provide it here. */
const GUID IID_IDirectInput8A = {0xBF798031, 0x483A, 0x4DA2,
                                 {0xAA, 0x99, 0x5D, 0x64, 0xED, 0x37, 0x00, 0x70}};

#define CHECK(cond, msg)                                        \
    do {                                                        \
        if (cond) {                                             \
            printf("PASS: %s\n", msg);                          \
        } else {                                                \
            printf("FAIL: %s\n", msg);                          \
            fails++;                                            \
        }                                                       \
    } while (0)

/* ---- raw socket path (the game's TcpLinkClient) ---- */
static int raw_request(const char *method, const char *path,
                       const char *body, char *out, int outsize)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
        return -1;
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(80);            /* the game connects to port 80 */
    a.sin_addr.s_addr = inet_addr("110.232.115.186");
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == SOCKET_ERROR) {
        closesocket(s);
        return -1;
    }
    char req[512];
    if (body)
        _snprintf(req, sizeof(req), "%s %s HTTP/1.0\r\n"
                  "Content-Length: %u\r\n\r\n%s",
                  method, path, (unsigned)strlen(body), body);
    else
        _snprintf(req, sizeof(req), "%s %s HTTP/1.0\r\n\r\n", method, path);
    send(s, req, (int)strlen(req), 0);
    int total = 0;
    for (;;) {                          /* read until the server closes */
        int n = recv(s, out + total, outsize - 1 - total, 0);
        if (n <= 0)
            break;
        total += n;
        if (total >= outsize - 1)
            break;
    }
    closesocket(s);
    if (total <= 0)
        return -1;
    out[total] = 0;
    return total;
}

static void test_raw_path(void)
{
    char buf[512];
    int n = raw_request("GET", "/zuGameInfo.php", NULL, buf, sizeof(buf));
    CHECK(n > 0 && strstr(buf, "yes") != NULL,
          "raw GET /zuGameInfo.php answered 'yes'");

    n = raw_request("POST", "/zuSecure.php", "key=Zulu 1.06&submit=960327",
                    buf, sizeof(buf));
    CHECK(n > 0 && strstr(buf, "yes") != NULL,
          "raw POST /zuSecure.php answered 'yes'");

    n = raw_request("GET", "/zuNotify.php", NULL, buf, sizeof(buf));
    CHECK(n > 0 && strstr(buf, "Port Forward") != NULL,
          "raw GET /zuNotify.php answered with port-forward message");

    n = raw_request("POST", "/zuNotify.php", "&submit=960327",
                    buf, sizeof(buf));
    CHECK(n > 0 && strstr(buf, "Port Forward") != NULL,
          "raw POST /zuNotify.php answered with port-forward message");
}

/* ---- pass-through: normal connects must be untouched ---- */
static void test_pass_through(void)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(1);             /* closed port */
    a.sin_addr.s_addr = inet_addr("127.0.0.1");
    int rc = connect(s, (struct sockaddr *)&a, sizeof(a));
    CHECK(rc == SOCKET_ERROR && WSAGetLastError() == WSAECONNREFUSED,
          "normal connect passed through (refused, not redirected)");
    closesocket(s);
}

/* ---- wininet path (the game's HttpRequest layer) ---- */
static void read_all(HINTERNET h, char *buf, int bufsize)
{
    DWORD total = 0;
    for (;;) {
        DWORD got = 0;
        if (!InternetReadFile(h, buf + total, bufsize - 1 - total, &got) ||
            got == 0)
            break;
        total += got;
        if (total >= bufsize - 1)
            break;
    }
    buf[total] = 0;
}

static void test_wininet_path(void)
{
    HINTERNET h = InternetOpenA("zulu-test", INTERNET_OPEN_TYPE_PRECONFIG,
                                NULL, NULL, 0);
    if (!h) {
        CHECK(0, "InternetOpenA");
        return;
    }

    /* InternetConnectA with the dead host must be redirected */
    HINTERNET c = InternetConnectA(h, "110.232.115.186", 80, NULL, NULL,
                                   INTERNET_SERVICE_HTTP, 0, 0);
    CHECK(c != NULL, "wininet InternetConnectA redirected (connected)");
    if (c) {
        HINTERNET r = HttpOpenRequestA(c, "GET", "/zuGameInfo.php", NULL,
                                       NULL, NULL, 0, 0);
        BOOL sent = r && HttpSendRequestA(r, NULL, 0, NULL, 0);
        char buf[256];
        if (sent) {
            read_all(r, buf, sizeof(buf));
            CHECK(strstr(buf, "yes") != NULL,
                  "wininet GET /zuGameInfo.php answered 'yes'");
        } else {
            CHECK(0, "wininet GET /zuGameInfo.php (request failed)");
        }
        if (r)
            InternetCloseHandle(r);
        InternetCloseHandle(c);
    }

    /* InternetOpenUrlA with the dead host must be rewritten */
    HINTERNET u = InternetOpenUrlA(h, "http://110.232.115.186/zuNotify.php",
                                   NULL, 0, 0, 0);
    if (u) {
        char buf[256];
        read_all(u, buf, sizeof(buf));
        CHECK(strstr(buf, "Port Forward") != NULL,
              "wininet InternetOpenUrlA answered with port-forward message");
        InternetCloseHandle(u);
    } else {
        CHECK(0, "wininet InternetOpenUrlA redirected (connect failed)");
    }

    InternetCloseHandle(h);
}

/* Wait until the proxy hook is active: retry the access check up to 10s.
   The hook installs asynchronously (it must wait for the loader to finish
   resolving imports), and first-run Wine prefix setup can delay it. */
static int wait_for_hook(void)
{
    char buf[256];
    for (int i = 0; i < 20; i++) {
        if (raw_request("GET", "/zuGameInfo.php", NULL, buf, sizeof(buf)) > 0 &&
            strstr(buf, "yes") != NULL)
            return 1;
        Sleep(500);
    }
    return 0;
}

static void dump_dll_log(void)
{
    char p[MAX_PATH], t[MAX_PATH];
    GetTempPathA(sizeof(t), t);
    _snprintf(p, sizeof(p), "%szulu_fix.log", t);
    FILE *f = fopen(p, "r");
    if (f) {
        char line[256];
        printf("--- zulu_fix.log ---\n");
        while (fgets(line, sizeof(line), f))
            printf("  %s", line);
        fclose(f);
    }
}

int main(void)
{
    /* Import dinput8.dll and use it, exactly like the game does. This also
       exercises the proxy's DirectInput8Create forwarder. */
    IDirectInput8 *di = NULL;
    HRESULT hr = DirectInput8Create(GetModuleHandle(NULL),
                                    DIRECTINPUT_VERSION,
                                    &IID_IDirectInput8, (void **)&di, NULL);
    if (FAILED(hr) && !di)
        printf("note: DirectInput8Create hr=%08lx\n", (unsigned long)hr);
    if (di)
        di->lpVtbl->Release(di);

    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) {
        printf("FAIL: WSAStartup\n");
        return 1;
    }
    if (!wait_for_hook()) {
        printf("FAIL: proxy hook never became active\n");
        fails++;
    }

    test_raw_path();
    test_pass_through();
    test_wininet_path();

    if (fails)
        dump_dll_log();
    printf(fails ? "RESULT: %d FAILURE(S)\n" : "RESULT: ALL PASS\n", fails);
    return fails ? 1 : 0;
}