// the agent loop. one generation per step, each under its own grammar.
//   PLAN -> for each NEED: CODE -> run -> (FIX)* -> OBSERVE -> FACT  -> ANSWER
// after a FACT the KV cache is rewound to just after PLAN. only the fact line moves forward.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "loop.h"
#include "grammar.h"
#include "parse.h"
#include "runner.h"
#include "embed.h"
#include <ctype.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

// first `lines` lines and at most `bytes` bytes of s, for feeding back to the model
static void head(const char *s, int lines, size_t bytes, char *out, size_t cap) {
    size_t n = 0;
    for (const char *p = s; *p && n + 1 < cap && n < bytes && lines > 0; p++) { out[n++] = *p; if (*p == '\n') lines--; }
    out[n] = 0;
    if (*s && n < strlen(s)) snprintf(out + n, cap - n, "%s...\n", n && out[n - 1] == '\n' ? "" : "\n");
}

typedef struct { engine *e; const loop_opts *o; char *facts; size_t facts_cap; int seed; char *last; } ctx;

// one user turn plus one grammar-bound reply. logs tokens and time. temp rises with each retry so a retry is not a replay.
static int step(ctx *c, const char *tag, const char *user, const char *gbnf, float temp, char *out, size_t cap) {
    if (engine_turn_user(c->e, user) < 0) return -1;
    double t0 = now();
    int n = engine_gen(c->e, gbnf, temp, 1000 + c->seed++, gbnf == G_CODE ? 800 : 400, out, cap);   // programs get more room
    if (n < 0) return -1;
    engine_turn_end(c->e);
    if (c->o->log) fprintf(c->o->log, "[%s] %d tok %.1fs ctx=%d\n%s\n", tag, engine_last_gen_tokens(c->e), now() - t0, engine_pos(c->e), out);
    return n;
}

static void add_fact(ctx *c, const char *f) {
    size_t n = strlen(c->facts);
    snprintf(c->facts + n, c->facts_cap - n, "- %s\n", f);
    if (c->last) { n = strlen(c->last); snprintf(c->last + n, 4096 - n, "- %s\n", f); }
}

// does the task ask for a yes or no? a cheap look at its first word and its shape.
static int yes_no_task(const char *t) {
    static const char *w[] = { "is ", "are ", "does ", "do ", "has ", "have ", "can ", "should ", "will ", "did ", "was ", "were ", 0 };
    char lo[64]; size_t i; for (i = 0; i < sizeof lo - 1 && t[i]; i++) lo[i] = t[i] >= 'A' && t[i] <= 'Z' ? t[i] + 32 : t[i]; lo[i] = 0;
    for (int k = 0; w[k]; k++) if (!strncmp(lo, w[k], strlen(w[k]))) return 1;
    return strstr(lo, " yes or no") != NULL;
}

// facts list too long: ask the model to merge it. keeps every number, drops repeats and chatter.
static void squash_facts(ctx *c) {
    char user[8192], out[4096];
    int base = engine_pos(c->e);
    snprintf(user, sizeof user, "Facts:\n%sRewrite this list shorter. Merge lines about the same thing. Keep every number and name. One fact per line, format '- <fact>'.", c->facts);
    if (step(c, "squash", user, G_LINES, c->o->temp, out, sizeof out) > 0 && strlen(out) < strlen(c->facts))
        snprintf(c->facts, c->facts_cap, "%s", out);
    engine_rewind(c->e, base);
}

// keep the last few exchanges. drop the oldest line pair when full.
static void add_history(session *ss, const char *q, const char *a) {
    char line[1024];
    snprintf(line, sizeof line, "Q: %.300s\nA: %.300s\n", q, a);
    while (strlen(ss->history) + strlen(line) >= sizeof ss->history) {
        char *nl = strchr(ss->history, '\n'); nl = nl ? strchr(nl + 1, '\n') : NULL;
        if (!nl) { ss->history[0] = 0; break; }
        memmove(ss->history, nl + 1, strlen(nl + 1) + 1);
    }
    strcat(ss->history, line);
}

// printable text? valid UTF-8, no control bytes but tab, newline, return. an unfilled buffer fails this.
static int is_text(const char *s, size_t n) {
    for (size_t i = 0; i < n;) {
        unsigned char c = s[i];
        if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') return 0;
        if (c == 0x7f) return 0;
        int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (!len || i + len > n) return 0;
        for (int k = 1; k < len; k++) if (((unsigned char)s[i + k] & 0xc0) != 0x80) return 0;
        i += len;
    }
    return 1;
}

// one line, single spaces, trimmed. for turning raw program output into a fact
static void squeeze(const char *in, char *out, size_t cap) {
    size_t n = 0; int sp = 1;
    for (; *in && n + 1 < cap; in++) {
        if (*in == ' ' || *in == '\n' || *in == '\r' || *in == '\t') { if (!sp) out[n++] = ' '; sp = 1; }
        else { out[n++] = *in; sp = 0; }
    }
    while (n && out[n - 1] == ' ') n--;
    out[n] = 0;
}

// "'X' undeclared": if X is a libc name we know the header for, prepend it and try again without the model.
static const char *HDR[][2] = {
    {"errno","errno.h"}, {"ENOENT","errno.h"}, {"EACCES","errno.h"}, {"EEXIST","errno.h"},
    {"strerror","string.h"}, {"strlen","string.h"}, {"memset","string.h"}, {"strcpy","string.h"}, {"strcmp","string.h"}, {"strstr","string.h"}, {"strtok","string.h"},
    {"exit","stdlib.h"}, {"malloc","stdlib.h"}, {"free","stdlib.h"}, {"atoi","stdlib.h"}, {"getenv","stdlib.h"}, {"EXIT_FAILURE","stdlib.h"}, {"EXIT_SUCCESS","stdlib.h"},
    {"printf","stdio.h"}, {"fopen","stdio.h"}, {"snprintf","stdio.h"}, {"perror","stdio.h"}, {"FILE","stdio.h"},
    {"sysconf","unistd.h"}, {"getuid","unistd.h"}, {"gethostname","unistd.h"}, {"close","unistd.h"}, {"read","unistd.h"}, {"write","unistd.h"}, {"getpid","unistd.h"}, {"access","unistd.h"}, {"_SC_NPROCESSORS_ONLN","unistd.h"}, {"_SC_PAGESIZE","unistd.h"},
    {"open","fcntl.h"}, {"O_RDONLY","fcntl.h"}, {"O_WRONLY","fcntl.h"}, {"O_CREAT","fcntl.h"},
    {"stat","sys/stat.h"}, {"mkdir","sys/stat.h"}, {"S_ISDIR","sys/stat.h"}, {"statvfs","sys/statvfs.h"},
    {"time","time.h"}, {"localtime","time.h"}, {"strftime","time.h"}, {"getpwuid","pwd.h"}, {"opendir","dirent.h"}, {"DIR","dirent.h"},
    {"uname","sys/utsname.h"}, {"sysinfo","sys/sysinfo.h"}, {"getifaddrs","ifaddrs.h"}, {"syslog","syslog.h"}, {"LOG_INFO","syslog.h"},
    {"socket","sys/socket.h"}, {"AF_INET","sys/socket.h"}, {"sockaddr_in","netinet/in.h"}, {"inet_ntop","arpa/inet.h"}, {"getaddrinfo","netdb.h"},
    {"PATH_MAX","limits.h"}, {"INT_MAX","limits.h"}, {"bool","stdbool.h"}, {"true","stdbool.h"}, {"uint64_t","stdint.h"}, {"int64_t","stdint.h"}, {"size_t","stddef.h"},
    {"isdigit","ctype.h"}, {"isspace","ctype.h"}, {"sqrt","math.h"}, {"getrusage","sys/resource.h"}, {"gettimeofday","sys/time.h"}, {"waitpid","sys/wait.h"},
    {0, 0} };
static int add_missing_include(const char *err, char *code, size_t cap) {
    const char *q = strstr(err, "error: '"); if (!q) return 0;
    q += 8; const char *e = strchr(q, '\''); if (!e || !strstr(e, "undeclared")) return 0;
    char name[64]; size_t n = e - q; if (n >= sizeof name) return 0;
    memcpy(name, q, n); name[n] = 0;
    for (int i = 0; HDR[i][0]; i++) if (!strcmp(HDR[i][0], name)) {
        char inc[64]; snprintf(inc, sizeof inc, "#include <%s>\n", HDR[i][1]);
        if (strstr(code, inc)) return 0;                    // already there, the error is something else
        if (strlen(code) + strlen(inc) + 1 > cap) return 0;
        memmove(code + strlen(inc), code, strlen(code) + 1); memcpy(code, inc, strlen(inc));
        return 1;
    }
    return 0;
}

// example programs. each file starts with "// q: <question>". the one with the most words in common with the need is shown.
// small models copy an example far better than they follow an instruction, and the example carries the right probe.
static struct { char q[256]; char *code; } EX[256]; static int n_ex = -1;
static void load_examples(void) {
    n_ex = 0;
    char path[1200]; snprintf(path, sizeof path, "%s/examples/index.txt", jb_root());
    FILE *idx = fopen(path, "r"); if (!idx) return;
    char name[128];
    while (n_ex < 256 && fgets(name, sizeof name, idx)) {
        name[strcspn(name, "\r\n")] = 0;
        snprintf(path, sizeof path, "%s/examples/%s.c", jb_root(), name);
        FILE *f = fopen(path, "r"); if (!f) continue;
        char *code = calloc(1, 4096); size_t n = fread(code, 1, 4095, f); fclose(f); code[n] = 0;
        if (strncmp(code, "// q: ", 6)) { free(code); continue; }
        char *nl = strchr(code, '\n'); if (!nl) { free(code); continue; }
        *nl = 0; snprintf(EX[n_ex].q, sizeof EX[n_ex].q, "%s", code + 6);
        EX[n_ex].code = nl + 1; n_ex++;
    }
    fclose(idx);
}
static int is_stop(const char *w) {
    // grammar words, and verbs or adjectives so generic that sharing one means nothing
    static const char *sw[] = { "the","how","what","many","much","this","that","does","have","has","are","is","on","of","in","to","a","an","and","for","with","by","my","it","its","which","do","from","at","be",
                                "open","big","large","small","get","check","current","all","any","there","some","each","than","more","less","most","least","into","out","up","over","about","like","using","use","right","now","or","was","were","can","should","tell","me","please","give","show","list","run","many", 0 };
    for (int i = 0; sw[i]; i++) if (!strcmp(w, sw[i])) return 1;
    return 0;
}
static int words(const char *s, char out[32][32]) {
    int n = 0, len = 0; char w[32];
    for (;; s++) {
        if (isalnum((unsigned char)*s) || *s == '/' || *s == '_') { if (len < 31) w[len++] = tolower((unsigned char)*s); }
        else {
            w[len] = 0;
            if (len >= 2 && !is_stop(w) && n < 32) {
                // a light stem so mount/mounts/mounted and address/addresses collapse together.
                if (len > 5 && !strcmp(w + len - 3, "ing")) w[len - 3] = 0;                          // running -> runn
                else if (len > 4 && !strcmp(w + len - 2, "ed")) w[len - 2] = 0;                       // mounted -> mount
                else if (len > 4 && w[len-1]=='s' && w[len-2]=='e' && w[len-3]=='s') w[len-2] = 0;    // addresses -> address, processes -> process
                else if (len > 3 && w[len - 1] == 's' && w[len - 2] != 's') w[len - 1] = 0;           // mounts -> mount, volumes -> volume
                strcpy(out[n++], w);
            }
            len = 0; if (!*s) break;
        }
    }
    return n;
}
// a word shared with many examples says little. "size" and "bytes" once picked the page-size example for a
// directory question. each shared word counts 1/(number of examples containing it). the total must clear 0.7,
// and either two words are shared or the one shared word is unique to that example. else no example:
// a wrong example is worse than none.
static double rarity(const char *w) {
    char b[32][32]; int df = 0;
    for (int i = 0; i < n_ex; i++) { int nb = words(EX[i].q, b); for (int y = 0; y < nb; y++) if (!strcmp(w, b[y])) { df++; break; } }
    return df ? 1.0 / df : 0;
}
const char *best_example(const char *need, const char **q) {
    if (n_ex < 0) load_examples();
    // two shared words above a low floor, or one word unique to a single example. the highest score wins.
    char a[32][32], b[32][32]; int na = words(need, a), best = -1; double best_score = 0.0;   // no absolute floor; eligibility is by shared words
    for (int i = 0; i < n_ex; i++) {
        int nb = words(EX[i].q, b), shared = 0; double score = 0;
        for (int x = 0; x < na; x++) for (int y = 0; y < nb; y++) if (!strcmp(a[x], b[y])) { score += rarity(a[x]); shared++; break; }
        if (getenv("JB_DEBUG_EX") && score > 0) fprintf(stderr, "  %.2f shared=%d  %s\n", score, shared, EX[i].q);
        // eligible: two shared words (a real signal even if common), or one word rare enough to be in <= 2 examples.
        if ((shared >= 2 || (shared == 1 && score >= 0.5)) && score > best_score) { best_score = score; best = i; }
    }
    *q = best >= 0 ? EX[best].q : NULL;
    return best >= 0 ? EX[best].code : NULL;
}

#define HELPERS "Helpers in <jb.h>: char *jb_read_file(path); int jb_write_file(path,text,append); char *jb_run(cmd); " \
    "char **jb_lines(text,&n); char *jb_line_starting(text,prefix); int jb_count_lines_with(text,needle); char *jb_kv(text,key,sep); " \
    "long long jb_number_in(s); char *jb_trim(s); char **jb_dir_list(path,&n); int jb_exists(path); long long jb_file_size(path). "

// --confirm: show the program, ask. no terminal means no.
static int confirmed(const char *code) {
    if (!isatty(0)) { fprintf(stderr, "refusing to run an action without a terminal. drop --confirm for unattended runs.\n"); return 0; }
    fprintf(stderr, "---- about to run ----\n%s---- run it? [y/N] ", code);
    char a[16]; return fgets(a, sizeof a, stdin) && (a[0] == 'y' || a[0] == 'Y');
}

// answer one NEED, or perform one DO, with a program. returns 0 and a fact line, or -1.
static int solve(ctx *c, char kind, const char *need, char *fact, size_t cap) {
    char user[8192], out[8192], code[8192], prev[8192] = "", obs[4096], oh[2048], eh[2048];
    int base = engine_pos(c->e);
    // the closest example goes in both paths. a DO needs it more than a NEED: sending and writing are where
    // the model invents a socket that connects and then lies about sending. see 09-actions.md.
    const char *exq = NULL, *ex = best_example(need, &exq);
    if (c->o->log && ex) fprintf(c->o->log, "[example] %s\n", exq);
    char block[5120] = "";
    if (ex) snprintf(block, sizeof block, "A working program for the similar request \"%s\":\n```c\n%s```\n", exq, ex);
    int win = c->o->host && strstr(c->o->host, "windows") != NULL;
    if (kind == 'D')
    snprintf(user, sizeof user,
             "Known facts:\n%s\n%sWrite one C99 program that does this: %s\n"
             "Rules: one file, only libc and <jb.h>, no input. " HELPERS
             "Do exactly that and nothing else, following the example above if there is one. Use C calls for files, directories, "
             "sockets and syslog: to send, open a socket and write to it, do not just connect. "
             "Where only a command does it, run it with jb_run and check its exit status. "
             "Then print one line saying what it actually did, with paths and counts.%s "
             "Exit 0 on success. On failure print the reason to stderr and exit 1. Code only.",
             *c->facts ? c->facts : "- none yet\n", block, need,
             win ? " On Windows: jb_win_service_control(name,\"start\"|\"stop\") from <jb_win.h>, else powershell -NoProfile -Command." : "");
    else
    snprintf(user, sizeof user,
             "Known facts:\n%s\n%sWrite one C99 program that answers: %s\n"
             "Rules: one file, only libc and <jb.h>, print the answer on stdout, exit 0, no input. " HELPERS "%sCode only.",
             *c->facts ? c->facts : "- none yet\n", block, need,
             ex ? "Follow the example above. " :
             win ? "On Windows: #include <jb_win.h> gives jb_win_user, jb_win_elevated, jb_win_privileges, jb_win_services, jb_win_service_status. For anything else run powershell -NoProfile -Command with ConvertTo-Json. " :
             (c->o->host && strstr(c->o->host, "linux") ? "Use the C library where it has the answer: sysconf, statvfs, getpwuid, uname, getifaddrs, fopen on files including /proc and /sys. Where only a command has it, run it with jb_run and parse its output. " : "Use the C library where it has the answer. Where only a command has it, run it with jb_run and parse its output. "));
    for (int attempt = 0; attempt < c->o->max_attempts; attempt++) {
        float temp = c->o->temp + 0.25f * attempt;
        if (step(c, "code", user, G_CODE, temp, out, sizeof out) < 0) return -1;
        if (parse_code(out, code, sizeof code) <= 0) {   // ran out of tokens before the closing fence
            if (c->o->log) fprintf(c->o->log, "[long] no complete program, not run\n");
            snprintf(user, sizeof user, "That program was too long and was cut off at 800 tokens. Write a shorter program that prints only the answer. Code only.");
            continue;
        }
        if (!strcmp(code, prev)) {   // a replay. do not run it again, push the model off the spot.
            if (c->o->log) fprintf(c->o->log, "[same] identical program, not run\n");
            snprintf(user, sizeof user, "That is the same program. It did not work. Use a different function or file. Code only.");
            continue;
        }
        snprintf(prev, sizeof prev, "%s", code);
        if (kind == 'D' && c->o->confirm && !confirmed(code)) { engine_rewind(c->e, base); fact[0] = 0; return 0; }

        run_result r;
        if (run_c(code, c->o->run_timeout_ms, 4096, &r)) return -1;
        if (c->o->log) fprintf(c->o->log, "[run] status=%d signal=%d timeout=%d out=%zuB err=%zuB\n", r.status, r.signal, r.timed_out, r.out_len, r.err_len);
        if (r.timed_out) {   // a search over a big tree can be slow and still right. one retry with 4x the budget, no model involved.
            run_free(&r);
            if (c->o->log) fprintf(c->o->log, "[slow] retrying with %d ms\n", 4 * c->o->run_timeout_ms);
            if (run_c(code, 4 * c->o->run_timeout_ms, 4096, &r)) return -1;
            if (c->o->log) fprintf(c->o->log, "[run] status=%d signal=%d timeout=%d out=%zuB err=%zuB\n", r.status, r.signal, r.timed_out, r.out_len, r.err_len);
        }
        head(r.out, 40, 1500, oh, sizeof oh);
        head(r.err, 20, 1500, eh, sizeof eh);

        if (r.compile_error) {
            if (c->o->log) fprintf(c->o->log, "[cerr] %s", eh);
            if (add_missing_include(eh, code, sizeof code)) {    // deterministic repair, no model, does not count as an attempt
                if (c->o->log) fprintf(c->o->log, "[include] added the missing header, retrying\n");
                run_free(&r);
                snprintf(prev, sizeof prev, "%s", code);
                if (run_c(code, c->o->run_timeout_ms, 4096, &r)) return -1;
                if (c->o->log) fprintf(c->o->log, "[run] status=%d signal=%d timeout=%d out=%zuB err=%zuB\n", r.status, r.signal, r.timed_out, r.out_len, r.err_len);
                head(r.out, 40, 1500, oh, sizeof oh); head(r.err, 20, 1500, eh, sizeof eh);
            }
        }
        if (r.compile_error) {
            const char *hint = strstr(eh, "include file") && strstr(eh, "not found")
                ? "That header does not exist here. Available: stdio.h stdlib.h string.h ctype.h errno.h math.h time.h unistd.h fcntl.h dirent.h "
                  "pwd.h grp.h signal.h ifaddrs.h netdb.h arpa/inet.h sys/stat.h sys/statvfs.h sys/utsname.h sys/sysinfo.h sys/resource.h sys/time.h sys/socket.h\n"
                : strstr(eh, "unresolved reference")
                ? "Use a different libc function for the same job.\n"
                : "";
            snprintf(user, sizeof user, "Compiler errors:\n%s%sWrite the whole program again with the error fixed. Code only.", eh, hint);
            run_free(&r);
            continue;
        }
        if (strstr(eh, "read-only mode:")) {   // refused by policy. retrying cannot help.
            if (c->o->log) fprintf(c->o->log, "[refused] %s", eh);
            run_free(&r); engine_rewind(c->e, base); fact[0] = 0; return 0;
        }
        if (kind == 'D' && r.status != 0 && !r.timed_out && !r.signal) {
            if (c->o->log) fprintf(c->o->log, "[failed] exit %d: %s", r.status, *eh ? eh : "(no stderr)\n");
            snprintf(user, sizeof user, "The program exited %d.\n%s%sFix it. Code only.", r.status, *eh ? "stderr:\n" : "", eh);
            run_free(&r);
            continue;
        }
        // nothing was observed: no verdict to ask for. a model asked anyway once answered "FACT: 200" after two timeouts.
        if (r.timed_out || r.signal) {
            if (c->o->log) fprintf(c->o->log, "[%s] asking for a fix\n", r.timed_out ? "hung" : "crashed");
            if (r.timed_out) snprintf(user, sizeof user, "The program ran over %d ms and was killed. It probably waited on something that never came, "
                                      "or did too much work. Rewrite it so it finishes fast. Code only.", 4 * c->o->run_timeout_ms);
            else snprintf(user, sizeof user, "The program crashed with signal %d. Check pointers, buffer sizes and return values. Rewrite it. Code only.", r.signal);
            run_free(&r);
            continue;
        }
        snprintf(obs, sizeof obs, "Exit code %d.\nstdout:\n%s%s%s", r.status, *oh ? oh : "(empty)\n", *eh ? "stderr:\n" : "", eh);
        char raw[256]; squeeze(r.out, raw, sizeof raw);
        int r_signal_or_timeout = r.timed_out || r.signal;
        int text = is_text(r.out, r.out_len);
        int short_out = r.out_len > 0 && r.out_len <= 200 && !r.timed_out && !r.signal && text;
        int clean = short_out && r.status == 0 && *raw;   // exit 0 and something printed
        if (!text && r.out_len) { size_t n = strlen(obs); snprintf(obs + n, sizeof obs - n, "The output is not text. It looks like an unfilled buffer.\n"); }
        run_free(&r);

        // exit 0 and a short answer: take it verbatim, do not ask. the model's verdicts are wrong in both directions
        // often enough that a deterministic rule beats them here. it still judges anything that looks off.
        if (clean) { snprintf(fact, cap, "%s", raw); if (c->o->log) fprintf(c->o->log, "[fact] %s (verbatim)\n", raw); engine_rewind(c->e, base); return 0; }
        // nothing printed: no verdict to ask for either. the model once accepted an empty line as a fact.
        if (!*raw && !r_signal_or_timeout) {
            if (c->o->log) fprintf(c->o->log, "[empty] no output, asking for a fix\n");
            snprintf(user, sizeof user, kind == 'D' ? "The program printed nothing. It must print one line saying what it did. Rewrite it. Code only."
                                                    : "The program printed nothing. It must print the answer on stdout. Rewrite it. Code only.");
            continue;
        }

        // short output: the model only judges yes or no, and the raw output becomes the fact. a value cannot be paraphrased.
        // long output: the model must summarize.
        if (short_out)
            snprintf(user, sizeof user, "%sQuestion: %s\nReply FACT if the output answers the question. Reply 'FIX: <what to change>' if it does not.", obs, need);
        else
            snprintf(user, sizeof user, "%sQuestion: %s\nReply 'FACT: <the answer only, no comment>' if the output answers the question. "
                     "Reply 'FIX: <what to change>' if it does not.", obs, need);
        if (step(c, "observe", user, short_out ? G_VERDICT : G_OBSERVE, c->o->temp, out, sizeof out) < 0) return -1;
        char okind, line[512];
        if (parse_observe(out, &okind, line, sizeof line)) return -1;
        if (okind == 'F') { snprintf(fact, cap, "%s", short_out ? raw : line); engine_rewind(c->e, base); return 0; }
        snprintf(user, sizeof user, "Change needed: %s\nRewrite the program. Code only.", line);
    }
    engine_rewind(c->e, base);
    fact[0] = 0;   // no answer is not a fact
    return 0;
}

int loop_task(engine *e, session *ss, const char *task, const loop_opts *o, char *answer, size_t cap) {
    ctx c = { e, o, ss->facts, sizeof ss->facts, ss->tasks * 100, ss->last_facts };
    ss->last_facts[0] = 0; ss->last_failed = 0; ss->last_verdict[0] = 0;
    int base = engine_pos(e);   // right after the system prompt. every task starts and ends here.
    char user[8192], out[4096], needs[5][128], kinds[5], vals[5][512]; int n_vals = 0;
    int have = *ss->facts != 0;
    int n = 0, done = 0;

    // first: the whole task as one step. one program answers or does it. most tasks end here.
    snprintf(user, sizeof user,
             "%s%s%s%sTask: %s\nRestate the whole task as one step, keeping every path, name and address in it. "
             "'NEED: <question>' if it only asks for information. 'DO: <action>' if it changes something, sends, or reports. "
             "Reading a value and sending it is one DO.",
             *ss->history ? "Earlier:\n" : "", ss->history,
             have ? "Known facts:\n" : "", have ? ss->facts : "", task);
    if (step(&c, "plan1", user, G_PLAN_ONE, o->temp, out, sizeof out) < 0) return -1;
    n = parse_plan(out, needs, kinds, &done);
    if (n == 1) {
        char fact[512];
        if (solve(&c, kinds[0], needs[0], fact, sizeof fact)) return -1;
        if (o->log) fprintf(o->log, "[fact] %s\n", *fact ? fact : "(none, falling back to a plan)");
        if (*fact) {
            char labeled[700]; snprintf(labeled, sizeof labeled, "%s%s %s", kinds[0] == 'D' ? "did: " : "", needs[0], fact); add_fact(&c, labeled);
            snprintf(vals[0], sizeof vals[0], "%s", fact); n_vals = 1;
            goto answer;
        }
    }

    // fallback: break it up
    snprintf(user, sizeof user,
             "%s%s%s%sTask: %s\nBreak it into steps, one per line, up to %d. One is usually enough. "
             "'NEED: <question>' learns something from the machine. 'DO: <action>' changes something or reports somewhere. "
             "A DO may compute what it needs itself, so one DO is often the whole task. "
             "Each step must fit in one small C program. Ask only about the machine, never about the tool or C itself. "
             "Reading a value and sending it, or reading and writing, is one DO. Examples: NEED: How many lines in /var/log/syslog mention error? "
             "DO: Read ITSHARE_ENDPOINT from ~/.itsvcs and POST it to 203.0.113.10:8080%s",
             *ss->history ? "Earlier:\n" : "", ss->history,
             have ? "Known facts:\n" : "You know nothing about this machine yet.\n", have ? ss->facts : "",
             task, o->max_needs,
             have ? " Ask only for what is missing. If the facts already answer the task, reply NONE." : "");
    if (step(&c, "plan", user, have ? G_PLAN_NONE : G_PLAN, o->temp, out, sizeof out) < 0) return -1;
    n = parse_plan(out, needs, kinds, &done);
    if (n > o->max_needs) n = o->max_needs;
    n_vals = 0;
    // NEEDs first, DOs last, whatever order the model wrote them: an action should see every fact gathered
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (kinds[i] == 'D' && kinds[j] == 'N') {
        char t[128]; memcpy(t, needs[i], 128); memcpy(needs[i], needs[j], 128); memcpy(needs[j], t, 128);
        kinds[i] = 'N'; kinds[j] = 'D';
    }

    for (int i = 0; i < n; i++) {
        char fact[512];
        if (solve(&c, kinds[i], needs[i], fact, sizeof fact)) return -1;
        if (o->log) fprintf(o->log, "[fact] %s\n", *fact ? fact : "(none)");
        if (!*fact) ss->last_failed = 1;
        vals[i][0] = 0; n_vals = i + 1;
        if (*fact) { char labeled[700]; snprintf(labeled, sizeof labeled, "%s%s %s", kinds[i] == 'D' ? "did: " : "", needs[i], fact); add_fact(&c, labeled);  // the step is the label
                     snprintf(vals[i], sizeof vals[0], "%s", fact); }
    }
answer:;
    size_t fmax = ss->facts_max ? ss->facts_max : 1500;
    if (strlen(ss->facts) > fmax) squash_facts(&c);

    // pure actions: the did-lines are the answer. the model's sentence only ever restated them, badly.
    int n_do = 0, n_did = 0;
    for (int i = 0; i < n; i++) if (kinds[i] == 'D') { n_do++; if (vals[i][0]) n_did++; }
    if (n_do == n && n_do) {
        size_t used = snprintf(answer, cap, "%s\n", n_did == n_do ? "done." : n_did ? "partly done." : "not done.");
        for (int i = 0; i < n && used < cap; i++) used += snprintf(answer + used, cap - used, "- %s: %s\n", needs[i], vals[i][0] ? vals[i] : "failed");
        add_history(ss, task, answer);
        ss->tasks++;
        engine_rewind(e, base);
        return 0;
    }

    snprintf(user, sizeof user, "%s%sTask: %s\nFacts:\n%sAnswer the task in one sentence that uses the facts it needs.",
             *ss->history ? "Earlier:\n" : "", ss->history, task, *ss->facts ? ss->facts : "- none\n");
    if (step(&c, "answer", user, G_ANSWER, o->temp, answer, cap) < 0) return -1;
    { size_t n = strlen(answer); while (n && (answer[n-1] == ' ' || answer[n-1] == '\n')) answer[--n] = 0; if (n + 1 < cap) { answer[n] = '\n'; answer[n+1] = 0; } }
    // a yes/no task gets a verdict: one word, grammar-bound, from the facts. the exit status follows it.
    if (yes_no_task(task)) {
        char v[16];
        snprintf(user, sizeof user, "Task: %s\nFacts:\n%sAnswer the task with one word: yes, no, or unknown.", task, *ss->facts ? ss->facts : "- none\n");
        if (step(&c, "verdict", user, G_YESNO, 0.0f, v, sizeof v) > 0) { v[strcspn(v, "\n")] = 0; snprintf(ss->last_verdict, sizeof ss->last_verdict, "%s", v); }
    }

    // a small model will "improve" a value while writing prose. any fact value missing from the answer is shown verbatim under it.
    int shown = 0;
    for (int i = 0; i < n_vals; i++) if (vals[i][0] && !strstr(answer, vals[i])) {
        size_t n = strlen(answer);
        snprintf(answer + n, cap - n, "%s- %s %s\n", shown++ ? "" : "also learned:\n", needs[i], vals[i]);
        if (o->log) fprintf(o->log, "[check] value '%s' not in the answer verbatim, appended\n", vals[i]);
    }
    add_history(ss, task, answer);
    ss->tasks++;
    engine_rewind(e, base);
    return 0;
}
