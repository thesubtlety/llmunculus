// M2 driver: run one protocol step N times with different seeds, check every output parses.
// usage: gramtest model.gguf plan|code|free "task or need" [runs] [temp]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "engine.h"
#include "grammar.h"
#include "parse.h"
#include "host.h"

static const char *SYS_FMT =
    "You are a build agent on a %s machine. You learn about it by writing tiny C programs. "
    "Reply in the exact format asked. Short lines.";

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s model step text [runs] [temp]\n", argv[0]); return 1; }
    const char *step = argv[2], *text = argv[3];
    int runs = argc > 4 ? atoi(argv[4]) : 5;
    float temp = argc > 5 ? atof(argv[5]) : 0.6f;

    engine *e = engine_open_file(argv[1], 2048, 4);
    if (!e) { fprintf(stderr, "load failed\n"); return 1; }

    char user[2048];
    const char *g = NULL;
    if (!strcmp(step, "plan")) {
        g = G_PLAN;
        snprintf(user, sizeof user, "Task: %s\nYou know nothing about this machine yet. "
                 "What must you find out from it first? Up to 3 questions, one per line, format 'NEED: <question>'. "
                 "Questions a small C program could answer. No commands.", text);
    } else if (!strcmp(step, "code")) {
        g = G_CODE;
        snprintf(user, sizeof user, "Write one C99 program that answers: %s\n"
                 "Rules: one file, only libc, print the answer on stdout, exit 0, no input. Code only.", text);
    } else if (!strcmp(step, "turnfree")) {   // system via template, then engine_turn_user (with any think prefill), free reply
        snprintf(user, sizeof user, "%s", text);
    } else {
        snprintf(user, sizeof user, "%s", text);
    }

    char host[64], sys[512], prompt[4096];
    host_desc(host, sizeof host);
    snprintf(sys, sizeof sys, SYS_FMT, host);
    fprintf(stderr, "system: %s\n", sys);
    if (engine_chat(e, sys, user, prompt, sizeof prompt) < 0) { fprintf(stderr, "template failed\n"); return 1; }
    if (!strcmp(step, "turnfree")) { char *cut = strstr(prompt, engine_template_piece(e, 0)); if (cut) *cut = 0; }   // keep the system part only
    double t0 = now();
    int n_in = engine_feed(e, prompt);
    if (!strcmp(step, "turnfree")) { engine_turn_user(e, user); n_in = engine_pos(e); }
    double t1 = now();
    int base = engine_pos(e);
    fprintf(stderr, "prompt %d tok, %.1fs\n", n_in, t1 - t0);

    int ok = 0, tot_tok = 0, tot_rej = 0; double tot_s = 0;
    char out[8192], code[8192], needs[5][128], kinds[5];
    for (int r = 0; r < runs; r++) {
        engine_rewind(e, base);  // same prompt, fresh continuation
        double a = now();
        int n = engine_gen(e, g, temp, 1000 + r, 400, out, sizeof out);
        double b = now();
        int toks = engine_last_gen_tokens(e), rej = engine_last_rejects(e);
        tot_tok += toks; tot_s += b - a; tot_rej += rej;

        int good = 0;
        if (g == G_PLAN) { int done; int k = parse_plan(out, needs, kinds, &done); good = done || k > 0; }
        else if (g == G_CODE) good = parse_code(out, code, sizeof code) > 0;
        else good = n > 0;
        ok += good;
        printf("--- run %d: %s, %d tok, %d grammar rejects, %.1fs ---\n%s", r, good ? "ok" : "BAD", toks, rej, b - a, out);
        if (n > 0 && out[n - 1] != '\n') printf("\n");
    }
    printf("=== %d/%d parsed. %.1f tok/s gen. avg %d tok per run. grammar overruled %d of %d tokens ===\n",
           ok, runs, tot_tok / tot_s, tot_tok / runs, tot_rej, tot_tok);
    engine_close(e);
    return ok == runs ? 0 : 2;
}
