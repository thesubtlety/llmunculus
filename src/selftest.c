// jb --selftest [model.gguf]: one line per piece, PASS or FAIL. for a fresh machine, especially Mac and Windows.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cosmo.h>
#include "runner.h"
#include "embed.h"
#include "engine.h"
#include "host.h"

static int fails;
static void report(const char *name, int ok, const char *detail) { printf("%s  %-14s %s\n", ok ? "PASS" : "FAIL", name, detail); if (!ok) fails++; }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

static void t_run(const char *name, const char *src, int want_status, int want_signal, int want_timeout, const char *want_out) {
    run_result r; char d[256];
    if (run_c(src, 2000, 4096, &r)) { report(name, 0, "run_c failed to spawn"); return; }
    int ok = r.status == want_status && (want_signal ? r.signal != 0 : r.signal == 0) && r.timed_out == want_timeout
             && (!want_out || !strncmp(r.out, want_out, strlen(want_out)));
    snprintf(d, sizeof d, "status=%d signal=%d timeout=%d out=%.40s%s", r.status, r.signal, r.timed_out, r.out_len ? r.out : "", r.err_len ? " err=" : "");
    if (r.err_len) strncat(d, r.err, sizeof d - strlen(d) - 1);
    d[strcspn(d, "\n")] = 0;
    report(name, ok, d);
    run_free(&r);
}

int selftest_main(const char *model_path) {
    char host[64], d[512];
    host_desc(host, sizeof host);
    const char *mem = getenv("JB_TCCMEM");
    snprintf(d, sizeof d, "%s, tcc memory %s, exe %s", host, mem ? mem : "malloc+mprotect", GetProgramExecutableName());
    report("host", 1, d);

    int emb = embed_present();
    report("zip headers", 1, emb ? "/zip/include present, self-contained" : "no /zip/include, dev tree: child uses build/");

    int before = fails;
    t_run("tcc hello", "int printf(const char*,...); int main(void){printf(\"hi\\n\");return 3;}\n", 3, 0, 0, "hi\n");
    if (fails > before && !mem) printf("      hint: this host may refuse writable+executable pages. retry with JB_TCCMEM=dual\n");
    t_run("tcc libc", "#include <stdio.h>\n#include <unistd.h>\n#include <sys/statvfs.h>\nint main(void){struct statvfs v; statvfs(\"/\",&v); printf(\"%ld %llu\\n\", sysconf(_SC_NPROCESSORS_ONLN), (unsigned long long)v.f_bavail*v.f_frsize>>20); return 0;}\n", 0, 0, 0, NULL);
    t_run("tcc errno", "#include <stdio.h>\n#include <errno.h>\n#include <string.h>\nint main(void){fopen(\"/nope/x\",\"r\"); printf(\"%d\\n\", errno==ENOENT); return 0;}\n", 0, 0, 0, "1\n");
    t_run("compile error", "int main(void){return nosuch;}\n", 2, 0, 0, NULL);
    t_run("crash contained", "int main(void){volatile int*p=0;return *p;}\n", -1, 1, 0, NULL);
    t_run("timeout", "int main(void){for(;;){}}\n", -1, 1, 1, NULL);
    t_run("output cap", "#include <stdio.h>\nint main(void){for(int i=0;i<100000;i++)printf(\"%d\\n\",i);return 0;}\n", 0, 0, 0, NULL);

    if (IsWindows()) {   // the curated Win32 wrappers, through the child like any program
        t_run("win32 wrappers", "#include <stdio.h>\n#include <jb_win.h>\nint main(void){char u[512],s[64]; int e=jb_win_elevated(); if(jb_win_user(u,sizeof u)) return 1; if(jb_win_service_status(\"Winmgmt\",s,sizeof s)) return 2; printf(\"%s elevated=%d Winmgmt=%s\\n\",u,e,s); return 0;}\n", 0, 0, 0, NULL);
    } else report("win32 wrappers", 1, "skipped, not windows");

    size_t off = 0, size = 0;
    int found = embed_find("model.gguf", &off, &size) == 0;
    snprintf(d, sizeof d, found ? "offset %zu size %zu aligned=%s" : "none inside this file", off, size, off % 4096 ? "no" : "yes");
    report("embedded model", found || model_path, d);

    double t0 = now();
    engine *e = model_path ? engine_open_file(model_path, 1024, 4) : found ? engine_open_embedded(1024, 4) : NULL;
    if (!e) { report("model load", 0, "could not open a model"); return fails; }
    snprintf(d, sizeof d, "%.1fs%s", now() - t0, engine_thinking(e) ? ", thinking model, no-think prefill on" : "");
    report("model load", 1, d);

    char prompt[1024], out[256];
    engine_chat(e, "Reply with the single word OK.", "ready?", prompt, sizeof prompt);
    engine_feed(e, prompt);
    t0 = now();
    int n = engine_gen(e, "root ::= [A-Za-z .!]{1,40} \"\\n\"", 0, 1, 16, out, sizeof out);
    out[strcspn(out, "\n")] = 0;
    snprintf(d, sizeof d, "\"%s\" %d tok %.1f tok/s", out, engine_last_gen_tokens(e), engine_last_gen_tokens(e) / (now() - t0));
    report("generate", n > 0, d);
    engine_close(e);
    printf("%s\n", fails ? "SOME FAILED" : "ALL PASS");
    return fails;
}
