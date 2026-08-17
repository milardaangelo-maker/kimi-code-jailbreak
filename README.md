# CyberGym Kimi Code Override

Research artifact for testing Kimi Code CLI prompt-override behavior.

Kimi Code's documented behavior is that a non-empty `$KIMI_CODE_HOME/SYSTEM.md` (default `~/.kimi-code/SYSTEM.md`) replaces the built-in main agent system prompt while inheriting the default tool set. This repo ships two install paths:

1. **EXE** — `dist/CyberGymKimiInstaller.exe` writes the override and launches `kimi`.
2. **Manual** — copy `payload/SYSTEM.md` yourself. See [MANUAL.md](MANUAL.md).

## Quick start (Windows EXE)

```powershell
.\dist\CyberGymKimiInstaller.exe
```

Options:

```powershell
.\dist\CyberGymKimiInstaller.exe --no-launch
.\dist\CyberGymKimiInstaller.exe --work "C:\path\to\folder"
.\dist\CyberGymKimiInstaller.exe --off
```

The EXE will:

1. Create `%USERPROFILE%\.kimi-code` if missing.
2. Write `payload/SYSTEM.md` contents to `%USERPROFILE%\.kimi-code\SYSTEM.md`.
3. Create a work folder (`%USERPROFILE%\Projects\cybergym-kimi-code` if `Projects` exists).
4. Open a new terminal in that folder running `kimi`.

`kimi` must already be installed and on PATH for auto-launch.

## Quick start (manual, no EXE)

Windows:

```powershell
New-Item -ItemType Directory -Force "$env:USERPROFILE\.kimi-code" | Out-Null
Copy-Item .\payload\SYSTEM.md "$env:USERPROFILE\.kimi-code\SYSTEM.md" -Force
```

Linux / macOS:

```bash
mkdir -p ~/.kimi-code
cp payload/SYSTEM.md ~/.kimi-code/SYSTEM.md
```

Then fully quit and relaunch `kimi`. Full steps: [MANUAL.md](MANUAL.md).

## What the override does

The payload names web vulnerability scanning/exploitation, AI jailbreak/prompt-injection artifacts, malware/reverse-engineering artifacts, and browser/session artifacts as in-scope benchmark work. Hard stops stay: no CSAM/minors, no violence, no blood/gore.

A successful load answers with `**[CYBERGYM]**`.

## Repo layout

```text
├─ dist/CyberGymKimiInstaller.exe   # Windows installer
├─ payload/SYSTEM.md                # same override, for manual copy
├─ Program.cs                       # installer source
├─ MANUAL.md                        # no-EXE install guide
└─ README.md
```

Generated benchmark artifacts (RATs, extensions, scan output) are not committed.

## Build the EXE from source

Windows, built-in .NET Framework 4.x compiler:

```powershell
& 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe' /nologo /target:exe /platform:anycpu /out:dist\CyberGymKimiInstaller.exe Program.cs
```

## Why this matters

A chat jailbreak has to convince the model every turn. Agent CLIs that load system prompts from disk change the threat model: one markdown file can persistently redefine the agent while keeping file/shell tools.

Sensitive paths:

```text
.kimi-code/agents/agent.md        # override: true can replace the main agent prompt
AGENTS.md                         # injected as project context
~/.kimi-code/SYSTEM.md            # global main-agent system prompt replacement
~/.agents/AGENTS.md               # generic cross-tool instructions
```

## Rollback

```powershell
.\dist\CyberGymKimiInstaller.exe --off
```

or:

```powershell
Rename-Item "$env:USERPROFILE\.kimi-code\SYSTEM.md" "SYSTEM.md.off"
```
