#include <stdio.h>
#include <cosmo.h>

int main(void) {
    const char *os = IsLinux() ? "linux" : IsXnu() ? "mac" : IsWindows() ? "windows" : "other";
#ifdef __x86_64__
    const char *arch = __builtin_cpu_supports("avx2") ? "x86_64 avx2" : "x86_64";
#else
    const char *arch = "aarch64";
#endif
    printf("hello from justabuilder\nos: %s  arch: %s\n", os, arch);
    return 0;
}
