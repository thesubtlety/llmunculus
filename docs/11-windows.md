# 11 windows

Status: built and linked, not yet run on Windows. `--selftest` on a Windows box has a `win32 wrappers` line that exercises it.

## three tiers

| tier | what | cost | reach |
|---|---|---|---|
| 1 | PowerShell through popen | none, a prompt hint | services, users, WMI and CIM, event logs, anything an admin types. JSON via ConvertTo-Json |
| 2 | curated Win32 wrappers, `<jb_win.h>` | one C file, ours | token identity, elevation, privileges, service status and control. fast, exact, no shell |
| 3 | COM | large | not built. WMI is reachable from tier 1, and vtables by hand in a small model's program is a poor bet |

Tier 1 is what the prompts say on a Windows host. Tier 2 is `src/win32.c`, six functions the model may call like any libc function. For a **remote** Windows host, the non-ssh path is WinRM: `Invoke-Command` and `Get-CimInstance -ComputerName`, see 14-remote.md.

## why a wrapper layer at all

Two calling conventions. tcc emits x86-64 SysV code: the first argument goes in `rdi`, the second in `rsi`. Win32 functions expect the Microsoft convention: `rcx`, `rdx`, and 32 bytes of shadow space on the stack. A tcc-compiled program calling `OpenProcessToken` directly would hand it garbage. cosmo's own libc is SysV everywhere and translates only at the Win32 boundary, which is why plain libc calls work from tcc programs on Windows. Our wrappers sit at that same boundary: compiled by cosmocc, SysV on the outside, Microsoft ABI on the inside.

## how the inside works

cosmo declares only what it uses of the NT API. `OpenProcessToken`, `GetCurrentProcess`, `LocalFree` and the token structs are there. `GetTokenInformation`, the SID lookups and the whole service manager are not. So `win32.c` fetches them at run time:

```c
typedef int32_t (__msabi *f_GetTokenInformation)(int64_t, uint32_t, void *, uint32_t, uint32_t *);
f_GetTokenInformation gti = GetProcAddress(LoadLibraryA("advapi32.dll"), "GetTokenInformation");
```

`__msabi` is cosmo's spelling of `__attribute__((ms_abi))`. GCC then emits the Microsoft convention for calls through that pointer. On the aarch64 half of the binary the attribute is empty, which is fine: there is no Windows arm64 target, and `IsWindows()` is false there.

Strings are UTF-16 on the Windows side. `tprecode8to16` and `tprecode16to8` from cosmo convert at the edges, so the model's program sees UTF-8 like everywhere else.

## what the six functions do

| function | Win32 underneath | note |
|---|---|---|
| `jb_win_user` | GetTokenInformation(TokenUser), LookupAccountSidW, ConvertSidToStringSidW | `DOMAIN\name S-1-5-...` |
| `jb_win_elevated` | GetTokenInformation(TokenElevation) | 1 for an admin token |
| `jb_win_privileges` | GetTokenInformation(TokenPrivileges), LookupPrivilegeNameW | one line each, enabled or disabled |
| `jb_win_services` | OpenSCManagerW, EnumServicesStatusExW | one line each, name and state |
| `jb_win_service_status` | OpenServiceW, QueryServiceStatusEx | running, stopped, start pending... |
| `jb_win_service_control` | StartServiceW, ControlService | start or stop. refused in `--read-only` |

On any other OS every one returns -1 with `ENOSYS`. The model only hears about them when the host is Windows.

## how to verify

On a Windows box: `llmunculus.exe --selftest`. The `win32 wrappers` line compiles a program that prints the user, elevation and the state of the Winmgmt service. Then a task: `llmunculus.exe "which services are stopped that are set to start automatically"`, which should land in tier 1, and `"am I running elevated"`, which should land in tier 2.

## what broke

Nothing yet. It has not run. Expect the first Windows run to teach something.
