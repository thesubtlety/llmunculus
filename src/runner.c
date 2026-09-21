// the parent: spawn ourselves with --run, pipe the source in, collect stdout and stderr, enforce a timeout.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <time.h>
#include <cosmo.h>
#include "runner.h"

static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000; }

struct buf { char *p; size_t n, cap; };
// keep reading past the cap so the child never blocks, but stop storing
static int slurp(int fd, struct buf *b) {
    char tmp[4096];
    ssize_t k = read(fd, tmp, sizeof tmp);
    if (k <= 0) return -1;
    size_t room = b->cap > b->n ? b->cap - b->n : 0;
    size_t take = (size_t)k < room ? (size_t)k : room;
    if (take) { b->p = realloc(b->p, b->n + take + 1); memcpy(b->p + b->n, tmp, take); b->n += take; b->p[b->n] = 0; }
    return 0;
}

int run_c(const char *src, int timeout_ms, size_t cap, run_result *r) {
    memset(r, 0, sizeof *r);
    int in[2], out[2], err[2];
    if (pipe(in) || pipe(out) || pipe(err)) return -1;

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, in[0], 0);
    posix_spawn_file_actions_adddup2(&fa, out[1], 1);
    posix_spawn_file_actions_adddup2(&fa, err[1], 2);
    for (int i = 0; i < 2; i++) { posix_spawn_file_actions_addclose(&fa, in[i]); posix_spawn_file_actions_addclose(&fa, out[i]); posix_spawn_file_actions_addclose(&fa, err[i]); }
    char *exe = GetProgramExecutableName();
    char *argv[] = { exe, "--run", NULL };
    int pid;
    int rc = posix_spawn(&pid, exe, &fa, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(in[0]); close(out[1]); close(err[1]);
    if (rc) { close(in[1]); close(out[0]); close(err[0]); errno = rc; return -1; }

    // source first. the child reads all of stdin before it writes anything, so this cannot deadlock.
    for (size_t off = 0, len = strlen(src); off < len;) { ssize_t k = write(in[1], src + off, len - off); if (k <= 0) break; off += k; }
    close(in[1]);

    struct buf ob = { NULL, 0, cap }, eb = { NULL, 0, cap };
    struct pollfd pf[2] = { { out[0], POLLIN, 0 }, { err[0], POLLIN, 0 } };
    long deadline = now_ms() + timeout_ms;
    int open_fds = 2;
    while (open_fds > 0) {
        long left = deadline - now_ms();
        if (left <= 0) { r->timed_out = 1; kill(pid, SIGKILL); break; }
        if (poll(pf, 2, (int)left) <= 0) continue;
        for (int i = 0; i < 2; i++) {
            if (pf[i].fd < 0 || !pf[i].revents) continue;
            if (slurp(pf[i].fd, i ? &eb : &ob) < 0) { close(pf[i].fd); pf[i].fd = -1; open_fds--; }
        }
    }
    if (pf[0].fd >= 0) close(pf[0].fd);
    if (pf[1].fd >= 0) close(pf[1].fd);

    int st = 0;
    waitpid(pid, &st, 0);
    if (WIFEXITED(st)) { r->status = WEXITSTATUS(st); r->compile_error = r->status == 2; }
    else { r->status = -1; if (WIFSIGNALED(st)) r->signal = WTERMSIG(st); }
    r->out = ob.p ? ob.p : calloc(1, 1); r->out_len = ob.n;
    r->err = eb.p ? eb.p : calloc(1, 1); r->err_len = eb.n;
    return 0;
}

void run_free(run_result *r) { free(r->out); free(r->err); r->out = r->err = NULL; }
