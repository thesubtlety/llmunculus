#include "parse.h"
#include <string.h>
#include <stdio.h>

int parse_plan(const char *t, char needs[5][128], char kinds[5], int *done) {
    *done = 0;
    if (!strncmp(t, "DONE", 4) || !strncmp(t, "NONE", 4)) { *done = 1; return 0; }
    int n = 0;
    while (n < 5 && (!strncmp(t, "NEED: ", 6) || !strncmp(t, "DO: ", 4))) {
        kinds[n] = t[0] == 'D' ? 'D' : 'N';
        t += t[0] == 'D' ? 4 : 6;
        const char *nl = strchr(t, '\n');
        int len = nl ? nl - t : (int)strlen(t);
        if (len > 127) len = 127;
        memcpy(needs[n], t, len); needs[n][len] = 0; n++;
        if (!nl) break;
        t = nl + 1;
    }
    return n;
}

int parse_code(const char *t, char *out, int cap) {
    const char *s = strstr(t, "```c\n");
    if (!s) return -1;
    s += 5;
    const char *e = strstr(s, "```");
    if (!e) return -1;
    int len = e - s;
    if (len >= cap) return -1;
    memcpy(out, s, len); out[len] = 0;
    return len;
}

int parse_observe(const char *t, char *kind, char *line, int cap) {
    const char *p;
    if (!strncmp(t, "FACT: ", 6)) { *kind = 'F'; p = t + 6; }
    else if (!strncmp(t, "FACT", 4)) { *kind = 'F'; p = t + 4; }
    else if (!strncmp(t, "FIX: ", 5)) { *kind = 'X'; p = t + 5; }
    else return -1;
    const char *nl = strchr(p, '\n');
    int len = nl ? nl - p : (int)strlen(p);
    if (len >= cap) len = cap - 1;
    memcpy(line, p, len); line[len] = 0;
    return 0;
}
