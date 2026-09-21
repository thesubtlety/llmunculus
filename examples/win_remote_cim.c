// q: on Windows, query a remote host's OS and memory without ssh, over WMI/CIM
#include <jb.h>
int main(void) {
    const char *host = "SRV01";   // WinRM/DCOM must be reachable; add -Credential in a domain
    char cmd[512]; snprintf(cmd, sizeof cmd,
        "powershell -NoProfile -Command \"Get-CimInstance -ComputerName %s -ClassName Win32_OperatingSystem | "
        "Select-Object CSName,Caption,@{n='FreeMB';e={[int]($_.FreePhysicalMemory/1024)}} | ConvertTo-Json -Compress\"", host);
    char *out = jb_run(cmd); if (!out || !*jb_trim(out)) { fprintf(stderr, "no response from %s\n", host); return 1; }
    printf("%s\n", jb_trim(out));   // one JSON line: CSName, Caption, FreeMB
    return 0;
}
