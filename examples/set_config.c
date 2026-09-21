// q: set a key to a value in a config file
#include <jb.h>
int main(void) {
    if (jb_exists("jbtest") != 2) mkdir("jbtest", 0755);
    const char *path = "jbtest/app.conf", *key = "max_connections", *val = "200";
    char *t = jb_read_file(path); if (!t) t = "";
    int n, found = 0; char **v = jb_lines(t, &n); char out[65536] = ""; size_t used = 0;
    for (int i = 0; i < n; i++) {
        if (!strncmp(v[i], key, strlen(key)) && v[i][strlen(key)] == '=') { used += snprintf(out + used, sizeof out - used, "%s=%s\n", key, val); found = 1; }
        else used += snprintf(out + used, sizeof out - used, "%s\n", v[i]);
    }
    if (!found) used += snprintf(out + used, sizeof out - used, "%s=%s\n", key, val);
    if (jb_write_file(path, out, 0) < 0) { perror("write"); return 1; }
    printf("%s %s=%s in %s\n", found ? "updated" : "added", key, val, path);
    return 0;
}
