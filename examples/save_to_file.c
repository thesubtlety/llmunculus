// q: save the full process list to a file and report how big it is
#include <jb.h>
int main(void) {
    const char *path = "processes.txt";
    char *all = jb_run("ps -eo pid,user,rss,comm 2>/dev/null");   // the whole list, any size
    if (!all) return 1;
    if (jb_write_file(path, all, 0) < 0) { perror(path); return 1; }   // written straight to disk, not through the model
    int n; jb_lines(all, &n);
    printf("wrote %lld bytes, %d lines to %s\n", jb_file_size(path), n, path);   // only this one-line receipt becomes the fact
    return 0;
}
