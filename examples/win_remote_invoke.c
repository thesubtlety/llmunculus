// q: on Windows, run a command on a remote host without ssh, over PowerShell remoting (WinRM)
#include <jb.h>
int main(void) {
    const char *host = "SRV01";
    char cmd[512]; snprintf(cmd, sizeof cmd,
        "powershell -NoProfile -Command \"Invoke-Command -ComputerName %s -ScriptBlock { "
        "(Get-Service | Where-Object Status -eq 'Stopped').Count } \"", host);   // WinRM on 5985/5986
    char *out = jb_run(cmd); if (!out) return 1;
    printf("%s stopped services on %s\n", jb_trim(out), host);
    return 0;
}
