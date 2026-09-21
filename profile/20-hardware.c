// section: hardware. cpu count and model, memory, swap
#include <jb.h>
int main(void) {
    const char *os = jb_os();
    jb_section("hardware");
    printf("%-16s %ld\n", "cpus", sysconf(_SC_NPROCESSORS_ONLN));
    if (!strcmp(os, "linux")) {
        char *ci = jb_read_file("/proc/cpuinfo"); if (ci) jb_row("cpu model", jb_kv(ci, "model name", ':'));
        char *mi = jb_read_file("/proc/meminfo");
        if (mi) { printf("%-16s %lld MB total, %lld MB available\n", "memory", jb_number_in(jb_kv(mi,"MemTotal",':'))/1024, jb_number_in(jb_kv(mi,"MemAvailable",':'))/1024);
                  printf("%-16s %lld MB\n", "swap", jb_number_in(jb_kv(mi,"SwapTotal",':'))/1024); }
    } else if (!strcmp(os, "macos")) {
        jb_row("cpu model", jb_first_line(jb_run("sysctl -n machdep.cpu.brand_string")));
        char *mem = jb_first_line(jb_run("sysctl -n hw.memsize")); if (mem) printf("%-16s %lld MB\n", "memory", jb_number_in(mem)/1048576);
    } else if (!strcmp(os, "windows")) {
        jb_row("cpu model", jb_first_line(jb_run("powershell -NoProfile -Command \"(Get-CimInstance Win32_Processor).Name\"")));
        char *mem = jb_first_line(jb_run("powershell -NoProfile -Command \"(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory\"")); if (mem) printf("%-16s %lld MB\n", "memory", jb_number_in(mem)/1048576);
    }
    return 0;
}
