// curated Win32 wrappers. our code, compiled by cosmocc, so it can call Microsoft-ABI functions that a tcc-compiled
// program cannot: tcc emits SysV calls, Win32 expects the Microsoft convention. cosmo declares only a few NT imports,
// so the rest are fetched from advapi32.dll at run time and called through __msabi function pointers.
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <libc/dce.h>
#include <libc/str/str.h>
#include <libc/nt/thunk/msabi.h>
#include <libc/nt/dll.h>
#include <libc/nt/runtime.h>
#include <libc/nt/memory.h>
#include <libc/nt/files.h>
#include <libc/nt/enum/tokeninformationclass.h>
#include <libc/nt/enum/accessmask.h>
#include <libc/nt/privilege.h>
#include <libc/nt/struct/luidandattributes.h>
#include <libc/nt/struct/tokenprivileges.h>
#include "jb_win.h"

typedef int32_t  (__msabi *f_GetTokenInformation)(int64_t, uint32_t, void *, uint32_t, uint32_t *);
typedef int32_t  (__msabi *f_LookupAccountSidW)(const char16_t *, void *, char16_t *, uint32_t *, char16_t *, uint32_t *, uint32_t *);
typedef int32_t  (__msabi *f_LookupPrivilegeNameW)(const char16_t *, void *, char16_t *, uint32_t *);
typedef int32_t  (__msabi *f_ConvertSidToStringSidW)(void *, char16_t **);
typedef int64_t  (__msabi *f_OpenSCManagerW)(const char16_t *, const char16_t *, uint32_t);
typedef int64_t  (__msabi *f_OpenServiceW)(int64_t, const char16_t *, uint32_t);
typedef int32_t  (__msabi *f_QueryServiceStatusEx)(int64_t, uint32_t, void *, uint32_t, uint32_t *);
typedef int32_t  (__msabi *f_EnumServicesStatusExW)(int64_t, uint32_t, uint32_t, uint32_t, void *, uint32_t, uint32_t *, uint32_t *, uint32_t *, const char16_t *);
typedef int32_t  (__msabi *f_ControlService)(int64_t, uint32_t, void *);
typedef int32_t  (__msabi *f_StartServiceW)(int64_t, uint32_t, const char16_t **);
typedef int32_t  (__msabi *f_CloseServiceHandle)(int64_t);

struct ServiceStatusProcess { uint32_t type, state, controls, exit, sexit, checkpoint, hint, pid, flags; };
struct EnumServiceStatusProcessW { char16_t *name, *display; struct ServiceStatusProcess st; };

static void *adv(const char *name) {            // a function from advapi32, or NULL
    static int64_t dll;
    if (!dll) dll = LoadLibraryA("advapi32.dll");
    return dll ? GetProcAddress(dll, name) : NULL;
}
static int not_here(void) { errno = ENOSYS; return -1; }
static int fail(void) { errno = EIO; return -1; }
static void to8(char *out, size_t cap, const char16_t *w) { tprecode16to8(out, cap, w); }
static void to16(char16_t *out, size_t cap, const char *s) { tprecode8to16(out, cap, s); }

static const char *state_name(uint32_t s) {
    static const char *n[] = { "?", "stopped", "start pending", "stop pending", "running", "continue pending", "pause pending", "paused" };
    return s < 8 ? n[s] : "?";
}

int jb_win_user(char *out, unsigned long cap) {
    if (!IsWindows()) return not_here();
    f_GetTokenInformation gti = adv("GetTokenInformation"); f_LookupAccountSidW las = adv("LookupAccountSidW"); f_ConvertSidToStringSidW css = adv("ConvertSidToStringSidW");
    if (!gti || !las) return fail();
    int64_t tok; if (!OpenProcessToken(GetCurrentProcess(), kNtTokenQuery, &tok)) return fail();
    unsigned char buf[512]; uint32_t n = 0;
    if (!gti(tok, kNtTokenUser, buf, sizeof buf, &n)) { CloseHandle(tok); return fail(); }
    CloseHandle(tok);
    void *sid = *(void **)buf;                      // TOKEN_USER: { SID_AND_ATTRIBUTES { PSID Sid; DWORD Attributes; } }
    char16_t name[256], dom[256]; uint32_t nl = 256, dl = 256, use = 0;
    char n8[256] = "?", d8[256] = "", s8[192] = "";
    if (las(NULL, sid, name, &nl, dom, &dl, &use)) { to8(n8, sizeof n8, name); to8(d8, sizeof d8, dom); }
    char16_t *sidw = NULL;
    if (css && css(sid, &sidw) && sidw) { to8(s8, sizeof s8, sidw); LocalFree(sidw); }
    snprintf(out, cap, "%s%s%s %s", d8, *d8 ? "\\" : "", n8, s8);
    return 0;
}

int jb_win_elevated(void) {
    if (!IsWindows()) return not_here();
    f_GetTokenInformation gti = adv("GetTokenInformation"); if (!gti) return fail();
    int64_t tok; if (!OpenProcessToken(GetCurrentProcess(), kNtTokenQuery, &tok)) return fail();
    uint32_t elev = 0, n = 0;
    int ok = gti(tok, 20 /* TokenElevation */, &elev, sizeof elev, &n);
    CloseHandle(tok);
    return ok ? (elev != 0) : fail();
}

int jb_win_privileges(char *out, unsigned long cap) {
    if (!IsWindows()) return not_here();
    f_GetTokenInformation gti = adv("GetTokenInformation"); f_LookupPrivilegeNameW lpn = adv("LookupPrivilegeNameW");
    if (!gti || !lpn) return fail();
    int64_t tok; if (!OpenProcessToken(GetCurrentProcess(), kNtTokenQuery, &tok)) return fail();
    unsigned char buf[4096]; uint32_t n = 0;
    if (!gti(tok, kNtTokenPrivileges, buf, sizeof buf, &n)) { CloseHandle(tok); return fail(); }
    CloseHandle(tok);
    struct NtTokenPrivileges *tp = (struct NtTokenPrivileges *)buf;
    size_t used = 0; out[0] = 0;
    for (uint32_t i = 0; i < tp->PrivilegeCount && used + 64 < cap; i++) {
        char16_t w[128]; uint32_t wl = 128; char s[128] = "?";
        if (lpn(NULL, &tp->Privileges[i].Luid, w, &wl)) to8(s, sizeof s, w);
        if (used >= cap) break;
        used += snprintf(out + used, cap - used, "%s %s\n", s, tp->Privileges[i].Attributes & kNtSePrivilegeEnabled ? "enabled" : "disabled");
    }
    return 0;
}

static int64_t open_scm(uint32_t access) { f_OpenSCManagerW o = adv("OpenSCManagerW"); return o ? o(NULL, NULL, access) : 0; }
static int64_t open_svc(int64_t scm, const char *name, uint32_t access) {
    f_OpenServiceW o = adv("OpenServiceW"); char16_t w[256]; to16(w, 256, name); return o ? o(scm, w, access) : 0;
}
static void close_h(int64_t h) { f_CloseServiceHandle c = adv("CloseServiceHandle"); if (c && h) c(h); }

int jb_win_service_status(const char *name, char *out, unsigned long cap) {
    if (!IsWindows()) return not_here();
    f_QueryServiceStatusEx q = adv("QueryServiceStatusEx"); if (!q) return fail();
    int64_t scm = open_scm(1 /* SC_MANAGER_CONNECT */); if (!scm) return fail();
    int64_t svc = open_svc(scm, name, 4 /* SERVICE_QUERY_STATUS */);
    if (!svc) { close_h(scm); errno = ENOENT; return -1; }
    struct ServiceStatusProcess st; uint32_t n = 0;
    int ok = q(svc, 0 /* SC_STATUS_PROCESS_INFO */, &st, sizeof st, &n);
    close_h(svc); close_h(scm);
    if (!ok) return fail();
    snprintf(out, cap, "%s", state_name(st.state));
    return 0;
}

int jb_win_services(char *out, unsigned long cap) {
    if (!IsWindows()) return not_here();
    f_EnumServicesStatusExW e = adv("EnumServicesStatusExW"); if (!e) return fail();
    int64_t scm = open_scm(1 | 4 /* CONNECT | ENUMERATE_SERVICE */); if (!scm) return fail();
    static unsigned char buf[256 * 1024]; uint32_t need = 0, count = 0, resume = 0;
    int ok = e(scm, 0 /* SC_ENUM_PROCESS_INFO */, 0x30 /* SERVICE_WIN32 */, 3 /* SERVICE_STATE_ALL */, buf, sizeof buf, &need, &count, &resume, NULL);
    close_h(scm);
    if (!ok && count == 0) return fail();
    struct EnumServiceStatusProcessW *s = (struct EnumServiceStatusProcessW *)buf;
    size_t used = 0; out[0] = 0;
    for (uint32_t i = 0; i < count && used + 300 < cap; i++) {
        char n8[256]; to8(n8, sizeof n8, s[i].name);
        if (used >= cap) break;
        used += snprintf(out + used, cap - used, "%s %s\n", n8, state_name(s[i].st.state));
    }
    return 0;
}

int jb_win_service_control(const char *name, const char *action) {
    if (!IsWindows()) return not_here();
    int start = !strcmp(action, "start"), stop = !strcmp(action, "stop");
    if (!start && !stop) { errno = EINVAL; return -1; }
    int64_t scm = open_scm(1); if (!scm) return fail();
    int64_t svc = open_svc(scm, name, start ? 0x10 /* SERVICE_START */ : 0x20 /* SERVICE_STOP */);
    if (!svc) { close_h(scm); errno = ENOENT; return -1; }
    int ok;
    if (start) { f_StartServiceW st = adv("StartServiceW"); ok = st && st(svc, 0, NULL); }
    else { f_ControlService cs = adv("ControlService"); struct ServiceStatusProcess s; ok = cs && cs(svc, 1 /* SERVICE_CONTROL_STOP */, &s); }
    close_h(svc); close_h(scm);
    return ok ? 0 : fail();
}
