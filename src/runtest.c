// M3 driver: five programs that must behave five different ways.
#include <stdio.h>
#include <string.h>
#include "runner.h"

static const char *CASES[][2] = {
    { "ok",       "#include <stdio.h>\n#include <unistd.h>\nint main(void){printf(\"cpus=%ld\\n\",sysconf(_SC_NPROCESSORS_ONLN));return 0;}\n" },
    { "exitcode", "int main(void){return 7;}\n" },
    { "compile",  "#include <stdio.h>\nint main(void){printf(\"%d\\n\", sysconf(_SC_NPROCESSORS));return 0;}\n" },
    { "crash",    "int main(void){volatile int *p = 0; return *p;}\n" },
    { "timeout",  "int main(void){for(;;){}}\n" },
    { "flood",    "#include <stdio.h>\nint main(void){for(int i=0;i<200000;i++)printf(\"line %d\\n\",i);return 0;}\n" },
    { "errno",    "#include <stdio.h>\n#include <errno.h>\n#include <string.h>\nint main(void){FILE*f=fopen(\"/nope\",\"r\");printf(\"%d %s\\n\",errno==ENOENT,strerror(errno));return f?1:0;}\n" },
};

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--run")) return child_main();
    int fails = 0;
    for (size_t i = 0; i < sizeof CASES / sizeof *CASES; i++) {
        run_result r;
        if (run_c(CASES[i][1], 1500, 4096, &r)) { perror("run_c"); return 1; }
        printf("--- %-8s status=%d signal=%d timeout=%d compile_error=%d out=%zuB err=%zuB\n",
               CASES[i][0], r.status, r.signal, r.timed_out, r.compile_error, r.out_len, r.err_len);
        if (r.out_len) printf("    out: %.60s%s\n", r.out, r.out_len > 60 ? "..." : "");
        if (r.err_len) printf("    err: %.100s%s\n", r.err, r.err_len > 100 ? "..." : "");
        run_free(&r);
    }
    return fails;
}
