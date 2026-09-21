# 13 explore

Status: done. `jb --explore` prints a host profile in a second or two, no model. Sections are per-platform probes, Linux tested, macOS and Windows written but unrun.

## why no model

Exploration does not need the model. The model's job is turning a novel question into a probe. A profile asks the same fixed questions every time, so the probes are known. `--explore` compiles one program, `src/explore.c`, through the same tcc child as any probe, runs it, and prints its output. Deterministic, fast, and it never invents a wrong `sysconf`.

It is also the strongest single test of the stack: `jb.h`, `jb_run` through a real shell, `/proc` parsing, sockets, and the tcc toolchain, all in one run. If `--explore` is clean, the plumbing is sound.

## what it reports

```
identity   hostname, os, kernel, arch, current user and home, uptime, local time
hardware   cpu count and model, memory total and available, swap
disk       every real mount: free, total, percent used
running    process count, failed units, top five by memory, docker containers
network    interfaces and IPv4 addresses, default gateway, listening ports, established count, dns
users      each human account (uid 1000-65000, login shell): shell, last login, home dir date
config     ssh root login, ip forwarding, firewall state
software   package count
```

Each section guards its own failures. A missing tool or file skips a line, never stops the profile. Commands go through `jb_run`, so `ss`, `ps`, `systemctl`, `docker`, `lastlog`, `ufw` are used where a file cannot answer, and their absence is silent.

## dates

Linux does not store an account creation date. `--explore` shows two dates per user instead: last login from `lastlog`, and the home directory's date as a proxy for when the account was set up. Last login reads "never" when sessions do not register with PAM, which key-based SSH on some setups does not, so the home date is the fallback signal.

## one file per section, cross-platform

`profile/` holds one probe per section: `10-identity.c`, `20-hardware.c`, `30-disk.c`, `40-running.c`, `50-network.c`, `60-users.c`, `70-config.c`, `80-software.c`. `--explore` reads `profile/index.txt` and runs them in that order. Adding a section is a new file and a line in the index. Fixing one platform is one edit in one file.

Each probe branches on `jb_os()`, which reads `uname` and returns `linux`, `macos`, `windows` or `unix`. So one probe carries all three implementations:

| section | linux | macos | windows |
|---|---|---|---|
| identity | /etc/os-release, /proc/uptime | sw_vers, sysctl kern.boottime | Get-CimInstance Win32_OperatingSystem |
| hardware | /proc/cpuinfo, /proc/meminfo | sysctl machdep, hw.memsize | Win32_Processor, Win32_ComputerSystem |
| disk | /proc/mounts + statvfs | mount + statvfs | Get-PSDrive |
| running | /proc, systemctl, ps, docker | ps | Get-Process |
| network | getifaddrs, /proc/net, ss | getifaddrs, route, netstat, scutil | Get-NetIPAddress, Get-NetTCPConnection |
| users | /etc/passwd, stat | dscl, stat | Get-LocalUser |
| config | sshd_config, /proc/sys, ufw | sshd_config, alf, csrutil | Get-NetFirewallProfile, RDP key |
| software | dpkg/rpm, apt | /Applications, brew | Get-Package |

`getifaddrs` and `statvfs` are the same call on Linux and macOS, so those halves share code. Everything Windows-specific and most macOS-specific work goes through `jb_run`, which reaches PowerShell or the BSD tools, so no section needs a symbol that only exists on one OS. That is why all eight compile under tcc on any host.

Tested: the Linux path of every section, on this box. Untested: the macOS and Windows branches, until a box runs them. When one is wrong, it is one file to fix, and `--explore` there will show which section failed.

## relation to the examples

`explore.c` is the example probes composed into one program: the same `/proc` reads, the same `jb_run` command patterns, the same helpers. Anything in the profile, the model can also answer as a single question, because the example for it exists. The profile is the fast, exact path; a one-off question is the model path.

## commands

```
jb --explore            # the embedded profile
JB_ROOT=build jb --explore   # dev tree
```
