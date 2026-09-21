# 14 remote

Status: built and tested through a local loopback. No sshd here for a real round trip, so the transport itself is unrun, but the orchestration is.

## two ways to reach another host

**1. Send the tool and run it there.** `jb --remote user@host [--explore | "task"]`. The tool scp's this executable to `/tmp` on the host, runs it there, prints the output, and deletes it. Nothing is installed on the remote. Because the binary is one portable static file, this works on any host the APE runs on.

```
llmunculus-thin --remote root@server --explore
```

profiles a remote box with no setup on it beyond ssh access. Use the thin build for this: `--explore` needs no model and the thin binary is 14 MB, so the copy is quick. A model task on the remote needs a model there, so either send the full 2.8 GB build or set `JB_MODEL` to a model already on the host.

The transport is overridable, for ports, keys, and jump hosts:

```
JB_SSH='ssh -p 2222 -i ~/.ssh/ops' JB_SCP='scp -P 2222 -i ~/.ssh/ops' llmunculus-thin --remote ops@host --explore
```

Tested with loopback stand-ins for scp and ssh that copy and run locally: the binary is placed, run with the given args, and cleaned up, and the profile comes back. The real ssh path is standard and unrun here only because this box has no sshd.

**2. Let a probe reach out over ssh.** For a single remote question without copying anything, a probe runs `ssh host 'command'` through `jb_run`. `examples/remote_uname.c` and `remote_disk.c` show the shape, with `BatchMode=yes` so a host without key access fails fast instead of hanging on a prompt. The model picks these for "the remote host over ssh" phrasings.

## which to use

| want | use |
|---|---|
| a full profile of a remote box | `--remote host --explore` |
| one remote fact, ssh keys set up | a task like "get the kernel of host X over ssh" |
| a model task run on the remote | `--remote host "task"` with a model on the remote |

The first is deterministic and complete. The second is a single command and needs no copy. The third is the heavy one.

## windows without ssh

Windows has native remote management that predates its OpenSSH client, and most Windows environments already have it on: WinRM. Two forms, both through `jb_run` with PowerShell, no ssh:

- **CIM/WMI**: `Get-CimInstance -ComputerName host -ClassName Win32_...`. Reads OS, memory, disk, services, almost anything, over DCOM or a CIM session. `win_remote_cim.c`.
- **PowerShell Remoting**: `Invoke-Command -ComputerName host -ScriptBlock { ... }`. Runs arbitrary PowerShell on the remote over WinRM (5985 plain, 5986 TLS). `win_remote_invoke.c`.

Both take `-Credential` in a domain. Neither needs anything installed on the target beyond WinRM, which domain machines usually have. The model routes "remote windows" phrasings to these.

## --remote over a non-ssh transport

`--remote`'s transport is `JB_SSH` and `JB_SCP`, and they are just command prefixes, so the bootstrap does not have to be ssh. On Windows-to-Windows you can copy over the admin share and run over WinRM by pointing them at PowerShell:

```
JB_SCP='<copy to \\host\c$\Windows\Temp via a wrapper>' JB_SSH='<Invoke-Command wrapper>' llmunculus --remote host --explore
```

The same override that adds an ssh port or key also swaps the whole transport. That is why `--remote` did not hard-code ssh.

## honest limits

- Both need ssh access already working: keys, or an agent. The tool does not handle passwords or host-key prompts; `BatchMode=yes` in the probes makes that a fast failure.
- `--remote` copies to `/tmp` and assumes the remote can execute it there. A noexec `/tmp` would need `JB_SSH`/a different path, not yet a flag.
- The real scp/ssh transport is unrun here. First real use will teach something, as every first run has.
