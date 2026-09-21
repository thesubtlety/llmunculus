#pragma once
#include <stddef.h>
typedef struct {
    int status;        // exit code, or -1 if killed by a signal or timeout
    int signal;        // signal number if killed, else 0
    int timed_out;
    int compile_error; // child exited 2: tcc rejected the source
    char *out, *err;   // heap, NUL terminated, capped
    size_t out_len, err_len;
} run_result;

// compile src with tcc inside a child process and run its main. never runs model code in this process.
int  run_c(const char *src, int timeout_ms, size_t cap, run_result *r);
void run_free(run_result *r);

// entry for the child. main() calls this when argv[1] is "--run".
int  child_main(void);
