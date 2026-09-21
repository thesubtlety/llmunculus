#include "host.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/utsname.h>

int host_desc(char *out, size_t cap) {
    struct utsname u;
    if (uname(&u)) return snprintf(out, cap, "unknown");
    for (char *p = u.sysname; *p; p++) *p = tolower((unsigned char)*p);
    return snprintf(out, cap, "%s %s", u.sysname, u.machine);
}
