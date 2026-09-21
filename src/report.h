#pragma once
#include <stddef.h>
// build the report document. facts is the task's "- ..." lines. verdict: "yes", "no", "unknown" or NULL.
int report_json(char *out, size_t cap, const char *host, const char *task, const char *answer, const char *facts, int failed, const char *verdict);
// send it. dest: stdout | file:PATH | syslog | http://... | https://... . returns 0 on success.
int report_send(const char *dest, const char *json, char *err, size_t errcap);
