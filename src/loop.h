#pragma once
#include <stdio.h>
#include "engine.h"

typedef struct {
    int max_needs;      // questions per task
    int max_attempts;   // code generations per question, fixes included
    int run_timeout_ms;
    float temp;
    FILE *log;          // step log, or NULL
    const char *host;   // "linux x86_64" etc. steers the code hint
    int confirm;        // ask on the terminal before running a DO program. off by default: cron
} loop_opts;

// what carries from task to task. text, not KV. the KV is rewound to the system prompt after every task.
typedef struct {
    char facts[4096];     // "- fact\n" lines
    char history[2048];   // "Q: ...\nA: ...\n" for the last few tasks
    size_t facts_max;     // squash facts when longer than this. 0 = default
    int tasks;
    char last_facts[4096]; // this task's "- ..." lines, for the report
    int last_failed;       // a step produced nothing
    char last_verdict[8];  // "yes", "no", "unknown", or "" when the task was not a yes/no question
} session;

// run one task end to end. answer is written to `answer`. returns 0 on success.
int loop_task(engine *e, session *ss, const char *task, const loop_opts *o, char *answer, size_t cap);
