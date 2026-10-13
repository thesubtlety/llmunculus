// llmunculus. usage: jb [-v] [model.gguf] ["task"]. no model: the one inside this file. no task: REPL. -v logs steps. --selftest [model] checks every piece.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "loop.h"
#include "runner.h"
#include "host.h"
#include "report.h"
#include "embed.h"
#include "discover.h"
// read a whole file (jb_slurp mirrors jb.h jb_read_file, but jb.c does not include jb.h)
static char *jb_slurp(const char *p){FILE *f=fopen(p,"rb");if(!f)return NULL;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);char *s=malloc(n+1);if(fread(s,1,n,f)!=(size_t)n){}s[n]=0;fclose(f);return s;}
#include <unistd.h>
#include <cosmo.h>
#include <signal.h>
#include <sys/stat.h>

// linux only: anonymous memory is ours, file-backed is the model paged in from this executable
static void mem_report(const char *when) {
    FILE *st = fopen("/proc/self/status", "r"); char l[128];
    if (!st) return;
    while (fgets(l, sizeof l, st)) if (!strncmp(l, "RssAnon", 7) || !strncmp(l, "RssFile", 7)) { l[strcspn(l, "\n")] = 0; fprintf(stderr, "[mem] %s  %s\n", when, l); }
    fclose(st);
}

int selftest_main(const char *model_path);
static int verbose;   // set early so the --remote fast path can log

// print, report, and pick the exit status. 0 ok, 2 verdict yes, 1 something failed.
static int finish(session *ss, const char *host, const char *task, const char *answer, int json, const char *report, int only_alert, int verbose) {
    int alert = !strcmp(ss->last_verdict, "yes") || ss->last_failed;
    char doc[8192], err[256];
    report_json(doc, sizeof doc, host, task, answer, ss->last_facts, ss->last_failed, *ss->last_verdict ? ss->last_verdict : NULL);
    if (json) puts(doc); else { fputs(answer, stdout); if (*ss->last_verdict) printf("verdict: %s\n", ss->last_verdict); }
    if (report && (!only_alert || alert)) {
        if (report_send(report, doc, err, sizeof err)) fprintf(stderr, "report to %s failed: %s\n", report, err);
        else if (verbose) fprintf(stderr, "[report] sent to %s\n", report);
    }
    return !strcmp(ss->last_verdict, "yes") ? 2 : ss->last_failed ? 1 : 0;
}

// a small file inside the zip, or NULL. task.txt and system.txt make a fixed-purpose build.
static char *zip_text(const char *name) {
    char path[64]; snprintf(path, sizeof path, "/zip/%s", name);
    FILE *f = fopen(path, "r"); if (!f) return NULL;
    char *s = calloc(1, 8192); size_t n = fread(s, 1, 8191, f); fclose(f);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = 0;
    return n ? s : (free(s), NULL);
}

// model precedence: command line .gguf, JB_MODEL, model.gguf beside the executable, the one inside it, one another tool downloaded
static engine *pick_model(const char *cli, int n_ctx, int threads, const char **used) {
    const char *env = getenv("JB_MODEL");
    char beside[1024]; snprintf(beside, sizeof beside, "%s", GetProgramExecutableName());
    char *slash = strrchr(beside, '/'); if (slash) strcpy(slash + 1, "model.gguf"); else strcpy(beside, "model.gguf");
    if (cli)                       { *used = cli;      return engine_open_file(cli, n_ctx, threads); }
    if (env && *env)               { *used = env;      return engine_open_file(env, n_ctx, threads); }
    if (access(beside, R_OK) == 0) { *used = "beside"; return engine_open_file(beside, n_ctx, threads); }
    size_t off, size;
    if (embed_find("model.gguf", &off, &size) == 0) { *used = "embedded"; return engine_open_embedded(n_ctx, threads); }
    const char *found = discover_best();
    if (!found) { *used = "none: no model on the command line, in $JB_MODEL, beside this file, inside it, or in another tool's cache"; return NULL; }
    *used = found;
    return engine_open_file(found, n_ctx, threads);
}

static void usage(const char *a0) {
    fprintf(stderr, "usage: %s [-v] [-t seconds] [model.gguf] [\"task\"]\n"
                    "  model: command line, else $JB_MODEL, else model.gguf beside this file, else the one inside it,\n"
                    "         else the best fit from ollama, LM Studio, the Hugging Face cache, llama.cpp, GPT4All, Jan\n"
                    "  task:  command line, else /zip/task.txt inside this file, else a REPL (/facts /reset /ctx /quit)\n"
                    "  -t     per-program run limit, default 5. a timed-out program gets one retry at 4x\n"
                    "  -a     code attempts per step, default 3\n"
                    "  --confirm    show each action program and ask before running it. off by default, for cron\n"
                    "  --read-only  programs may read and compute but not write, delete, run commands, or send\n"
                    "  --json       print the report document instead of the sentence\n"
                    "  --report DEST      also send it: stdout | file:PATH | syslog | http(s)://...\n"
                    "  --report-if-alert  only when the verdict is yes or a step failed\n"
                    "  exit status: 0 ok, 2 the task was a yes/no question and the answer is yes, 1 error\n"
                    "  --selftest [model.gguf]   check every piece\n"
                    "  --explore                 print a concise host profile, no model needed\n"
                    "  --list-models             the models other tools have downloaded, best fit first. the first is the default\n"
                    "  --use-model X             one of them: its number in that list, a name fragment, or a path\n"
                    "  --remote user@host ...    run on another host. see --remote with no arguments\n", a0);
}

static int ends_with(const char *s, const char *suf) { size_t n = strlen(s), m = strlen(suf); return n > m && !strcmp(s + n - m, suf); }

// the model this run would use, without loading it: JB_MODEL, model.gguf beside the executable, else a found one
static const char *local_model_path(void) {
    const char *env = getenv("JB_MODEL"); if (env && *env) return env;
    static char beside[1024]; snprintf(beside, sizeof beside, "%s", GetProgramExecutableName());
    char *slash = strrchr(beside, '/'); if (slash) strcpy(slash + 1, "model.gguf"); else strcpy(beside, "model.gguf");
    if (access(beside, R_OK) == 0) return beside;
    return discover_best();
}

static int sh(const char *cmd) { if (verbose) fprintf(stderr, "[remote] %s\n", cmd); return system(cmd); }

// wrap s in single quotes for the local shell, so whatever quoting it already carries reaches ssh untouched.
static char *sq(const char *s, char *out, size_t cap) {
    size_t k = 0; if (k < cap) out[k++] = '\'';
    for (; *s && k < cap - 5; s++) { if (*s == '\'') { memcpy(out + k, "'\\''", 4); k += 4; } else out[k++] = *s; }
    if (k < cap) out[k++] = '\'';
    out[k < cap ? k : cap - 1] = 0; return out;
}

// copy this binary to a host, run it there, print the output. no install on the remote. a model task uses, in order:
// the copy we sent before (same size), a model another tool left on the host, else our local pick sent once to ~/.cache/llmunculus.
static int remote_main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s --remote user@host [--explore | [--use-model X] [--send-model] \"task\"]\n"
                             "  copies this executable to the host over scp and runs it there. your ssh config, keys and agent apply.\n"
                             "  a model task reuses a model already on the host, else sends the local pick once to ~/.cache/llmunculus/model.gguf.\n"
                             "  --send-model sends ours even if the host has one. JB_SSH and JB_SCP override the transport, e.g. JB_SSH='ssh -p 2222 -i key'.\n", argv[0]); return 1; }
    const char *host = argv[2];
    const char *ssh = getenv("JB_SSH"), *scp = getenv("JB_SCP");
    // one authentication for the whole run: the first connection stays open as a master for the others. not on Windows OpenSSH.
    char mux[160] = "", keys[512] = "", sshd[768], scpd[768];
    if (!IsWindows()) snprintf(mux, sizeof mux, " -o ControlMaster=auto -o ControlPath=/tmp/jb-ssh-%%r@%%h:%%p -o ControlPersist=60");
    // JB_SSH_KEYS: space-separated key files, each -i in order, IdentitiesOnly so only these are offered (under the server's MaxAuthTries)
    const char *kl = getenv("JB_SSH_KEYS");
    if (kl && *kl) {
        size_t n = snprintf(keys, sizeof keys, " -o IdentitiesOnly=yes");
        char tmp[400]; snprintf(tmp, sizeof tmp, "%s", kl);
        for (char *t = strtok(tmp, " "); t && n < sizeof keys - 64; t = strtok(NULL, " ")) n += snprintf(keys + n, sizeof keys - n, " -i %s", t);
    }
    if (!ssh) { snprintf(sshd, sizeof sshd, "ssh%s%s", mux, keys); ssh = sshd; }
    if (!scp) { snprintf(scpd, sizeof scpd, "scp%s%s", mux, keys); scp = scpd; }

    // the remote invocation: default --explore, else the given args, each single-quoted for the remote shell. model flags stay local.
    const char *model_arg = NULL; int send = 0, need_model = argc > 3;
    char ra[6144] = ""; size_t k = 0;
    for (int i = 3; i < argc && k < sizeof ra - 300; i++) {
        if (!strcmp(argv[i], "--send-model")) { send = 1; continue; }
        if (!strcmp(argv[i], "--use-model") && i + 1 < argc) { model_arg = argv[++i]; continue; }
        if (ends_with(argv[i], ".gguf")) { model_arg = argv[i]; continue; }
        if (!strcmp(argv[i], "--explore")) need_model = 0;
        k += snprintf(ra + k, sizeof ra - k, " '");
        for (const char *c = argv[i]; *c && k < sizeof ra - 8; c++) { if (*c == '\'') k += snprintf(ra + k, sizeof ra - k, "'\\''"); else ra[k++] = *c; }
        k += snprintf(ra + k, sizeof ra - k, "'");
    }
    if (!*ra) snprintf(ra, sizeof ra, " --explore");

    char local[1024] = ""; size_t off, sz;
    if (need_model && embed_find("model.gguf", &off, &sz) == 0) need_model = 0;   // the full build carries its own
    if (need_model) {
        if (model_arg) { if (discover_pick(model_arg, local, sizeof local)) { fprintf(stderr, "no model matches %s. see --list-models\n", model_arg); return 1; } }
        else { const char *p = local_model_path(); if (p) snprintf(local, sizeof local, "%s", p); }
    }

    char self[1024]; snprintf(self, sizeof self, "%s", GetProgramExecutableName());
    char tmp[64]; snprintf(tmp, sizeof tmp, "/tmp/jb-remote-%d", (int)getpid());
    char cmd[8192];
    char q1[1100], q2[1200];
    snprintf(cmd, sizeof cmd, "%s %s %s:%s", scp, sq(self, q1, sizeof q1), host, tmp);
    if (sh(cmd)) { fprintf(stderr, "copy to %s failed\n", host); return 1; }

    const char *rmodel = "$HOME/.cache/llmunculus/model.gguf";
    char menv[128] = "";
    if (need_model) {
        // what the host has: our earlier copy (its size), and whether another tool left a usable model
        snprintf(cmd, sizeof cmd, "%s %s 'chmod +x %s; mkdir -p $HOME/.cache/llmunculus; if [ -f %s ]; then wc -c < %s; else echo 0; fi; %s --list-models >/dev/null 2>&1 && echo found || echo none'",
                 ssh, host, tmp, rmodel, rmodel, tmp);
        if (verbose) fprintf(stderr, "[remote] %s\n", cmd);
        FILE *p = popen(cmd, "r"); char l1[64] = "0", l2[64] = "none";
        if (p) { if (fgets(l1, sizeof l1, p)) fgets(l2, sizeof l2, p); pclose(p); }
        long long have = atoll(l1); int found = !strncmp(l2, "found", 5);
        struct stat st; long long want = *local && !stat(local, &st) ? (long long)st.st_size : -1;
        int reuse = want > 0 && have == want;
        int copy = *local && (send || (!reuse && !found));
        if (copy) {
            fprintf(stderr, "[remote] sending %s (%.1f GB) to %s:~/.cache/llmunculus/model.gguf, once\n", local, want / 1e9, host);
            snprintf(cmd, sizeof cmd, "%s %s %s:.cache/llmunculus/model.gguf", scp, sq(local, q1, sizeof q1), host);
            if (sh(cmd)) { fprintf(stderr, "model copy to %s failed\n", host); return 1; }
            snprintf(menv, sizeof menv, "JB_MODEL=%s ", rmodel);
        } else if (reuse || (have > 0 && !found)) {
            if (verbose) fprintf(stderr, "[remote] using the model already at %s:~/.cache/llmunculus/model.gguf (%lld bytes)\n", host, have);
            snprintf(menv, sizeof menv, "JB_MODEL=%s ", rmodel);
        } else if (!found) {
            fprintf(stderr, "no model on %s and none here to send. see --list-models\n", host);
            snprintf(cmd, sizeof cmd, "%s %s 'rm -f %s'", ssh, host, tmp); sh(cmd);
            return 1;
        } else if (verbose) fprintf(stderr, "[remote] %s has a model of its own\n", host);
    }
    char rcmd[7200]; snprintf(rcmd, sizeof rcmd, "chmod +x %s && %s%s%s; rm -f %s", tmp, menv, tmp, ra, tmp);   // ra already remote-quoted
    snprintf(cmd, sizeof cmd, "%s %s %s", ssh, host, sq(rcmd, q2, sizeof q2));   // quote once more for the local shell
    int rc = sh(cmd);
    return rc ? 1 : 0;
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);   // a child or socket peer dying mid-write must not kill the agent
    if (argc > 1 && !strcmp(argv[1], "--run")) return child_main();
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "-v")) verbose = 1;
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return selftest_main(argc > 2 ? argv[2] : NULL) ? 1 : 0;
    if (argc > 2 && !strcmp(argv[1], "-v") && !strcmp(argv[2], "--remote")) { argv++; argc--; }   // -v --remote reads naturally
    if (argc > 1 && !strcmp(argv[1], "--remote")) return remote_main(argc, argv);
    if (argc > 1 && !strcmp(argv[1], "--explore")) {   // host profile: run each profile/*.c section in order. no model.
        char idx[1200]; snprintf(idx, sizeof idx, "%s/profile/index.txt", jb_root());
        char *list = jb_slurp(idx);
        if (!list) { fprintf(stderr, "profile sections not found at %s\n", idx); return 1; }
        char *save = NULL;
        for (char *name = strtok_r(list, "\n", &save); name; name = strtok_r(NULL, "\n", &save)) {
            char path[1300]; snprintf(path, sizeof path, "%s/profile/%s.c", jb_root(), name);
            char *src = jb_slurp(path); if (!src) continue;
            run_result r;
            if (run_c(src, 15000, 1 << 16, &r)) { free(src); continue; }
            fputs(r.out, stdout);
            if (r.err_len) fprintf(stderr, "[%s] %.*s", name, (int)r.err_len, r.err);   // a broken section notes itself, others go on
            run_free(&r); free(src);
        }
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--list-models")) {   // what the scan finds, and refresh the cached pick
        model_cand c[64]; int n = discover_models(c, 64);
        for (int i = 0; i < n; i++) printf("%2d  %4.1f GB  %-40.40s %s\n", c[i].score, c[i].gb, c[i].name, c[i].path);
        if (!n) { printf("no usable model found\n"); return 1; }
        printf("default: %s  (cached in %s)\n", c[0].path, discover_cache_path());
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "--example")) {   // which example would this question get? no model needed
        const char *best_example(const char *, const char **); const char *q = NULL;
        for (int i = 2; i < argc; i++) { best_example(argv[i], &q); printf("%-60.60s -> %s\n", argv[i], q ? q : "(none)"); }
        return 0;
    }
    int ai = 1, timeout_s = 5, confirm = 0, attempts = 3, json = 0, only_alert = 0;
    const char *report = NULL; char use_model[1024] = "";
    for (; ai < argc && argv[ai][0] == '-'; ai++) {
        if (!strcmp(argv[ai], "-v")) verbose = 1;
        else if (!strcmp(argv[ai], "--json")) json = 1;
        else if (!strcmp(argv[ai], "--report") && ai + 1 < argc) report = argv[++ai];
        else if (!strcmp(argv[ai], "--report-if-alert")) only_alert = 1;
        else if (!strcmp(argv[ai], "--confirm")) confirm = 1;
        else if (!strcmp(argv[ai], "--read-only")) setenv("JB_READONLY", "1", 1);   // the child reads it
        else if (!strcmp(argv[ai], "-a") && ai + 1 < argc) attempts = atoi(argv[++ai]);
        else if (!strcmp(argv[ai], "-t") && ai + 1 < argc) timeout_s = atoi(argv[++ai]);
        else if (!strcmp(argv[ai], "--use-model") && ai + 1 < argc) {
            if (discover_pick(argv[++ai], use_model, sizeof use_model)) { fprintf(stderr, "no model matches %s. see --list-models\n", argv[ai]); return 1; }
        }
        else { usage(argv[0]); return 1; }
    }
    const char *path = *use_model ? use_model : NULL;
    if (argc > ai && strlen(argv[ai]) > 5 && !strcmp(argv[ai] + strlen(argv[ai]) - 5, ".gguf")) path = argv[ai++];
    const char *used;
    engine *e = pick_model(path, 8192, 4, &used);
    if (!e) { fprintf(stderr, "cannot load model (%s)\n", used); usage(argv[0]); return 1; }
    if (verbose) { fprintf(stderr, "[model] %s%s\n", used, engine_thinking(e) ? ", thinking model: replies opened with an empty <think> block" : ""); mem_report("after load"); }

    char host[64], sys[512];
    host_desc(host, sizeof host);
    char *sys_zip = zip_text("system.txt");
    if (sys_zip) snprintf(sys, sizeof sys, "%s", sys_zip);
    else snprintf(sys, sizeof sys, "You are an agent on a %s machine. You get things done by writing small C programs: "
                  "look things up, read and write files, compute, talk to services, run commands and read their output. "
                  "Reply in the exact format asked. Short lines.", host);
    char prompt[2048];
    if (engine_chat(e, sys, "ready?", prompt, sizeof prompt) < 0) return 1;
    char *cut = strstr(prompt, engine_template_piece(e, 0));   // keep only the system part
    if (cut) *cut = 0;
    engine_feed(e, prompt);
    if (verbose) fprintf(stderr, "[sys] %d tok%s\n", engine_pos(e), sys_zip ? " (from /zip/system.txt)" : "");

    loop_opts o = { 3, attempts, timeout_s * 1000, 0.3f, verbose ? stderr : NULL, host, confirm };
    session ss = { "", "", 0, 0 };
    const char *fm = getenv("JB_FACTS_MAX"); if (fm) ss.facts_max = atoi(fm);
    char answer[2048];

    const char *task = argc > ai ? argv[ai] : zip_text("task.txt");
    if (task) {   // one task, from the command line or baked in
        if (verbose && argc <= ai) fprintf(stderr, "[task] from /zip/task.txt\n");
        if (loop_task(e, &ss, task, &o, answer, sizeof answer)) { fprintf(stderr, "loop failed\n"); return 1; }
        int rc = finish(&ss, host, task, answer, json, report, only_alert, verbose);
        if (verbose) mem_report("at exit");
        engine_close(e);
        return rc;
    }

    // REPL. one model, one context, facts carry across tasks.
    char line[2048];
    for (;;) {
        fputs("> ", stdout); fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        line[strcspn(line, "\n")] = 0;
        if (!*line) continue;
        if (!strcmp(line, "/quit") || !strcmp(line, "/q")) break;
        if (!strcmp(line, "/facts")) { fputs(*ss.facts ? ss.facts : "- none\n", stdout); continue; }
        if (!strcmp(line, "/reset")) { ss.facts[0] = ss.history[0] = 0; ss.tasks = 0; puts("cleared"); continue; }
        if (!strcmp(line, "/ctx")) { printf("kv tokens now: %d (cache says %d)\n", engine_pos(e), engine_kv_pos(e)); mem_report("now"); continue; }
        if (loop_task(e, &ss, line, &o, answer, sizeof answer)) { puts("loop failed"); continue; }
        finish(&ss, host, line, answer, json, report, only_alert, verbose);
    }
    engine_close(e);
    return 0;
}
