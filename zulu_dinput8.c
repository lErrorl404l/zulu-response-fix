/*
 * zulu_dinput8.c - proxy DLL that fixes Zulu Response (Steam appid 401250).
 *
 * The game hardcodes its backend server as 110.232.115.186:80. That server is
 * dead and now returns 404 HTML, so the game's access check fails and the game
 * exits at the menu. This DLL revives the backend in-process.
 *
 * Drop as dinput8.dll into Binaries/Win32/. The game imports dinput8.dll and
 * loads it from the app directory before the system one, on Windows and Wine.
 *
 * 1. DirectInput8Create and friends are forwarded to the real dinput8
 *    (located via GetSystemDirectoryA - no hardcoded paths).
 * 2. The exe's import table entry for WSOCK32!connect (ordinal 4) is rewritten
 *    to point at my_connect, which redirects connections to
 *    110.232.115.186:80 to 127.0.0.1:8080. All other traffic passes through.
 *    (IAT patching: a pointer swap, no code modification - safe on Wine.)
 * 3. A minimal HTTP server runs on 127.0.0.1:8080 and answers the game's
 *    protocol: the access check expects "yes" (from the game's 2017 logs),
 *    notify requests expect the port-forward message.
 *
 * Build: i686-w64-mingw32-gcc -shared -O2 -o dinput8.dll \
 *            zulu_dinput8.c zulu_dinput8.def -lws2_32
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wininet.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <string.h>

/* Network-byte-order constants. */
#define DEAD_ADDR   0x0BA73E86EUL   /* bytes 6E E8 73 BA = 110.232.115.186 */
#define DEAD_PORT   0x5000          /* bytes 00 50 = 80 */
#define LOCAL_ADDR  0x0100007FUL    /* bytes 7F 00 00 01 = 127.0.0.1 */
#define LOCAL_PORT  0x901F          /* bytes 1F 90 = 8080 */

#define NOTIFY_MSG "To Host Games your router must Port Forward ports : 6500, 7777 to 7790, 13000, 27900 *$*"

#define IMAGE_ORDINAL_FLAG32 0x80000000
#define IMAGE_ORDINAL32(n) ((n) & 0xFFFF)

/* ------------------------------------------------------------------ */
/* Logging                                                             */
/* ------------------------------------------------------------------ */
static FILE *g_logf;

static void logmsg(const char *fmt, ...)
{
    if (!g_logf) {
        char p[MAX_PATH];
        GetTempPathA(sizeof(p), p);
        lstrcatA(p, "zulu_fix.log");
        g_logf = fopen(p, "a");
    }
    if (!g_logf)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_logf, fmt, ap);
    va_end(ap);
    fputc('\n', g_logf);
    fflush(g_logf);
}

/* ------------------------------------------------------------------ */
/* HTTP server                                                         */
/* ------------------------------------------------------------------ */
static const char *stristr(const char *haystack, const char *needle)
{
    for (; *haystack; haystack++) {
        const char *a = haystack, *b = needle;
        while (*a && *b &&
               tolower((unsigned char)*a) == tolower((unsigned char)*b))
            a++, b++;
        if (!*b)
            return haystack;
    }
    return NULL;
}

static void serve_client(SOCKET c)
{
    char buf[4096];
    int n = recv(c, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = 0;
        char first[96];
        int i;
        for (i = 0; i < n && i < 95 && buf[i] != '\r' && buf[i] != '\n'; i++)
            first[i] = buf[i];
        first[i] = 0;
        logmsg("server: %s", first);
        /* notify requests use "zuNotify.php": match case-insensitively */
        const char *body = (stristr(buf, "notify") != NULL) ? NOTIFY_MSG : "yes";
        char resp[1024];
        int rl = _snprintf(resp, sizeof(resp),
                           "HTTP/1.0 200 OK\r\nConnection: close\r\n"
                           "Content-Length: %u\r\n\r\n%s",
                           (unsigned)strlen(body), body);
        if (rl > 0)
            send(c, resp, rl, 0);
    }
    closesocket(c);
}

static DWORD WINAPI server_thread(LPVOID unused)
{
    (void)unused;
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) {
        logmsg("server: WSAStartup failed");
        return 1;
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        logmsg("server: socket failed %d", WSAGetLastError());
        return 1;
    }
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = LOCAL_PORT;
    a.sin_addr.s_addr = LOCAL_ADDR;
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) == SOCKET_ERROR) {
        logmsg("server: bind 127.0.0.1:8080 failed %d "
               "(server already running?)", WSAGetLastError());
        closesocket(s);
        return 1;
    }
    if (listen(s, 8) == SOCKET_ERROR) {
        logmsg("server: listen failed %d", WSAGetLastError());
        closesocket(s);
        return 1;
    }
    logmsg("server: listening on 127.0.0.1:8080");
    for (;;) {
        SOCKET c = accept(s, NULL, NULL);
        if (c == INVALID_SOCKET)
            break;
        serve_client(c);
    }
    closesocket(s);
    return 0;
}

/* ------------------------------------------------------------------ */
/* connect redirect                                                    */
/* ------------------------------------------------------------------ */
typedef int (WINAPI *connect_t)(SOCKET, const struct sockaddr *, int);
static connect_t g_real_connect;

static int WINAPI my_connect(SOCKET s, const struct sockaddr *name, int namelen)
{
    if (name && name->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)name;
        if (in->sin_port == DEAD_PORT && in->sin_addr.s_addr == DEAD_ADDR) {
            struct sockaddr_in tmp = *in;
            tmp.sin_port = LOCAL_PORT;
            tmp.sin_addr.s_addr = LOCAL_ADDR;
            logmsg("connect: redirected 110.232.115.186:80 -> 127.0.0.1:8080");
            return g_real_connect(s, (const struct sockaddr *)&tmp, namelen);
        }
    }
    return g_real_connect(s, name, namelen);
}

/*
 * Rewrite the import-address-table slot for one function in one module.
 * Works on both name imports and ordinal imports (WSOCK32!connect is
 * ordinal 4 in this game). Deferred until after the loader finishes,
 * otherwise the loader would overwrite the slot.
 */
static void patch_iat_slot(HMODULE mod, const char *dll, WORD ordinal,
                           const char *name, FARPROC new_fn)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)mod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((BYTE *)mod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return;
    DWORD imp_rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
                    .VirtualAddress;
    if (!imp_rva)
        return;
    IMAGE_IMPORT_DESCRIPTOR *desc =
        (IMAGE_IMPORT_DESCRIPTOR *)((BYTE *)mod + imp_rva);

    for (; desc->Name; desc++) {
        const char *dll_name = (const char *)mod + desc->Name;
        if (lstrcmpiA(dll_name, dll) != 0)
            continue;
        DWORD *int_ = (DWORD *)((BYTE *)mod + desc->OriginalFirstThunk);
        DWORD *iat = (DWORD *)((BYTE *)mod + desc->FirstThunk);
        if (!int_)
            int_ = iat;  /* no bound names: INT equals IAT */
        for (; *int_; int_++, iat++) {
            if (*int_ & IMAGE_ORDINAL_FLAG32) {
                if (IMAGE_ORDINAL32(*int_) != ordinal)
                    continue;
            } else {
                IMAGE_IMPORT_BY_NAME *ibn =
                    (IMAGE_IMPORT_BY_NAME *)((BYTE *)mod + *int_);
                if (name && lstrcmpiA((const char *)ibn->Name, name) != 0)
                    continue;
            }
            DWORD old;
            VirtualProtect(iat, sizeof(DWORD), PAGE_READWRITE, &old);
            *iat = (DWORD)new_fn;
            VirtualProtect(iat, sizeof(DWORD), old, &old);
            logmsg("hook: %s!%s slot @%p -> my_connect", dll,
                   name ? name : "#ord", iat);
            return;
        }
    }
    logmsg("hook: %s!%s slot not found in module", dll,
           name ? name : "#ord");
}

/* wininet redirects (the game's HTTP layer also uses WININET) */
typedef HINTERNET (WINAPI *iconnect_t)(HINTERNET, LPCWSTR, INTERNET_PORT,
                                       LPCWSTR, LPCWSTR, DWORD, DWORD, DWORD_PTR);
typedef HINTERNET (WINAPI *iconnecta_t)(HINTERNET, LPCSTR, INTERNET_PORT,
                                        LPCSTR, LPCSTR, DWORD, DWORD, DWORD_PTR);
typedef HINTERNET (WINAPI *iurl_t)(HINTERNET, LPCSTR, LPCSTR, DWORD, DWORD, DWORD_PTR);
typedef HINTERNET (WINAPI *iurlw_t)(HINTERNET, LPCWSTR, LPCWSTR, DWORD, DWORD, DWORD_PTR);

static iconnect_t g_real_iconnect;
static iconnecta_t g_real_iconnecta;
static iurl_t g_real_iurl;
static iurlw_t g_real_iurlw;

static DWORD WINAPI hook_thread(LPVOID unused);
static HINTERNET WINAPI my_InternetConnectA(HINTERNET, LPCSTR, INTERNET_PORT,
                                            LPCSTR, LPCSTR, DWORD, DWORD, DWORD_PTR);
static HINTERNET WINAPI my_InternetConnectW(HINTERNET, LPCWSTR, INTERNET_PORT,
                                            LPCWSTR, LPCWSTR, DWORD, DWORD, DWORD_PTR);
static HINTERNET WINAPI my_InternetOpenUrlA(HINTERNET, LPCSTR, LPCSTR,
                                            DWORD, DWORD, DWORD_PTR);
static HINTERNET WINAPI my_InternetOpenUrlW(HINTERNET, LPCWSTR, LPCWSTR,
                                            DWORD, DWORD, DWORD_PTR);

static DWORD WINAPI hook_thread(LPVOID unused)
{
    (void)unused;
    Sleep(1000);  /* let the loader finish resolving all imports */

    HMODULE ws = GetModuleHandleA("ws2_32.dll");
    if (!ws)
        ws = LoadLibraryA("ws2_32.dll");
    if (!ws) {
        logmsg("hook: ws2_32 not available");
        return 1;
    }
    g_real_connect = (connect_t)GetProcAddress(ws, "connect");
    if (!g_real_connect) {
        logmsg("hook: connect export not found");
        return 1;
    }

    HMODULE wi = GetModuleHandleA("wininet.dll");
    if (wi) {
        g_real_iconnecta = (iconnecta_t)GetProcAddress(wi, "InternetConnectA");
        g_real_iconnect = (iconnect_t)GetProcAddress(wi, "InternetConnectW");
        g_real_iurl = (iurl_t)GetProcAddress(wi, "InternetOpenUrlA");
        g_real_iurlw = (iurlw_t)GetProcAddress(wi, "InternetOpenUrlW");
    }

    HMODULE exe = GetModuleHandleA(NULL);
    patch_iat_slot(exe, "WSOCK32.dll", 4, NULL, (FARPROC)my_connect);
    patch_iat_slot(exe, "ws2_32.dll", 0, "connect", (FARPROC)my_connect);
    patch_iat_slot(exe, "WININET.dll", 0, "InternetConnectA",
                   (FARPROC)my_InternetConnectA);
    patch_iat_slot(exe, "WININET.dll", 0, "InternetConnectW",
                   (FARPROC)my_InternetConnectW);
    patch_iat_slot(exe, "WININET.dll", 0, "InternetOpenUrlA",
                   (FARPROC)my_InternetOpenUrlA);
    patch_iat_slot(exe, "WININET.dll", 0, "InternetOpenUrlW",
                   (FARPROC)my_InternetOpenUrlW);
    logmsg("hook: real connect is %p", g_real_connect);
    return 0;
}

/* ------------------------------------------------------------------ */
/* wininet redirects (the game's HTTP layer also uses WININET)         */
/* ------------------------------------------------------------------ */
#define DEAD_HOST "110.232.115.186"
#define LOCAL_HOST "127.0.0.1"

static HINTERNET WINAPI my_InternetConnectA(HINTERNET h, LPCSTR server,
        INTERNET_PORT port, LPCSTR u, LPCSTR p, DWORD svc, DWORD fl, DWORD_PTR cx)
{
    if (port == 80 && server && lstrcmpiA(server, DEAD_HOST) == 0) {
        logmsg("connect: wininet %s:80 -> 127.0.0.1:8080", server);
        server = LOCAL_HOST;
        port = 8080;
    }
    return g_real_iconnecta(h, server, port, u, p, svc, fl, cx);
}

static HINTERNET WINAPI my_InternetConnectW(HINTERNET h, LPCWSTR server,
        INTERNET_PORT port, LPCWSTR u, LPCWSTR p, DWORD svc, DWORD fl, DWORD_PTR cx)
{
    if (port == 80 && server && lstrcmpiW(server, L"110.232.115.186") == 0) {
        logmsg("connect: wininet %ls:80 -> 127.0.0.1:8080", server);
        server = L"127.0.0.1";
        port = 8080;
    }
    return g_real_iconnect(h, server, port, u, p, svc, fl, cx);
}

/*
 * Rewrite http://110.232.115.186[:80]/... to http://127.0.0.1:8080/...
 * The caller's URL string is read-only, so a copy is built.
 */
static LPCSTR rewrite_url(const char *url)
{
    static char buf[1024];
    const char *host = strstr(url, DEAD_HOST);
    if (!host)
        return url;
    size_t pre = (size_t)(host - url);
    const char *end = host + strlen(DEAD_HOST);
    if (*end == ':')
        end++;                    /* skip ":80" if present */
    if (pre + strlen(LOCAL_HOST) + 6 + strlen(end) < sizeof(buf)) {
        memcpy(buf, url, pre);
        strcpy(buf + pre, LOCAL_HOST ":8080");
        strcat(buf, end);
        logmsg("connect: wininet url rewritten to %s", buf);
        return buf;
    }
    return url;
}

static LPCWSTR rewrite_url_w(const wchar_t *url)
{
    static wchar_t buf[1024];
    const wchar_t *host = wcsstr(url, L"110.232.115.186");
    if (!host)
        return url;
    size_t pre = (size_t)(host - url);
    const wchar_t *end = host + wcslen(L"110.232.115.186");
    if (*end == L':')
        end++;
    if (pre + wcslen(L"127.0.0.1") + 6 + wcslen(end) < 1024) {
        memcpy(buf, url, pre * sizeof(wchar_t));
        wcscpy(buf + pre, L"127.0.0.1:8080");
        wcscat(buf, end);
        return buf;
    }
    return url;
}

static HINTERNET WINAPI my_InternetOpenUrlA(HINTERNET h, LPCSTR url,
        LPCSTR headers, DWORD hl, DWORD fl, DWORD_PTR cx)
{
    return g_real_iurl(h, rewrite_url(url), headers, hl, fl, cx);
}

static HINTERNET WINAPI my_InternetOpenUrlW(HINTERNET h, LPCWSTR url,
        LPCWSTR headers, DWORD hl, DWORD fl, DWORD_PTR cx)
{
    return g_real_iurlw(h, rewrite_url_w(url), headers, hl, fl, cx);
}

/* ------------------------------------------------------------------ */
/* dinput8 forwarders                                                  */
/* ------------------------------------------------------------------ */
static HMODULE g_real_dinput8;

static FARPROC real_proc(const char *n)
{
    if (!g_real_dinput8) {
        char sysdir[MAX_PATH];
        char path[MAX_PATH];
        if (GetSystemDirectoryA(sysdir, sizeof(sysdir)) &&
            _snprintf(path, sizeof(path), "%s\\dinput8.dll", sysdir) > 0)
            g_real_dinput8 = LoadLibraryA(path);
        if (!g_real_dinput8)
            logmsg("fw: cannot load real dinput8 (%d)", GetLastError());
    }
    return g_real_dinput8 ? GetProcAddress(g_real_dinput8, n) : NULL;
}

HRESULT WINAPI DirectInput8Create(HINSTANCE a, DWORD b, REFIID c,
                                  LPVOID *d, LPUNKNOWN e)
{
    typedef HRESULT (WINAPI *fn_t)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
    fn_t f = (fn_t)real_proc("DirectInput8Create");
    return f ? f(a, b, c, d, e) : E_FAIL;
}

HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    typedef HRESULT (WINAPI *fn_t)(REFCLSID, REFIID, LPVOID *);
    fn_t f = (fn_t)real_proc("DllGetClassObject");
    return f ? f(rclsid, riid, ppv) : CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    typedef HRESULT (WINAPI *fn_t)(void);
    fn_t f = (fn_t)real_proc("DllCanUnloadNow");
    return f ? f() : S_FALSE;
}

HRESULT WINAPI DllRegisterServer(void)
{
    typedef HRESULT (WINAPI *fn_t)(void);
    fn_t f = (fn_t)real_proc("DllRegisterServer");
    return f ? f() : E_FAIL;
}

HRESULT WINAPI DllUnregisterServer(void)
{
    typedef HRESULT (WINAPI *fn_t)(void);
    fn_t f = (fn_t)real_proc("DllUnregisterServer");
    return f ? f() : E_FAIL;
}

void WINAPI GetdfDIJoystick(void)
{
    typedef void (WINAPI *fn_t)(void);
    fn_t f = (fn_t)real_proc("GetdfDIJoystick");
    if (f)
        f();
}

HRESULT WINAPI ExportHookProc(void)
{
    typedef HRESULT (WINAPI *fn_t)(void);
    fn_t f = (fn_t)real_proc("ExportHookProc");
    return f ? f() : E_FAIL;
}

/* ------------------------------------------------------------------ */
/* Entry                                                               */
/* ------------------------------------------------------------------ */
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        logmsg("=== zulu fix: loaded into process ===");
        CreateThread(NULL, 0, server_thread, NULL, 0, NULL);
        CreateThread(NULL, 0, hook_thread, NULL, 0, NULL);
    }
    return TRUE;
}