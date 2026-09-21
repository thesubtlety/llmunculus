#pragma once
// pull "NEED: ..." and "DO: ..." lines out. kinds[i] is 'N' or 'D'. returns count, or 0 with *done=1 on "NONE".
int parse_plan(const char *text, char needs[5][128], char kinds[5], int *done);
// copy the body of a ```c fence into out. returns length or -1.
int parse_code(const char *text, char *out, int cap);
// "FACT: x" -> kind 'F', "FIX: x" -> kind 'X'. returns 0 on success.
int parse_observe(const char *text, char *kind, char *line, int cap);
