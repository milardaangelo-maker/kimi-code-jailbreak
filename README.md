# CyberGym Kimi Code Override Installer

Research artifact for testing Kimi Code CLI prompt-override behavior.

The installer writes a custom `SYSTEM.md` for Kimi Code. Kimi Code's documented behavior is that a non-empty `$KIMI_CODE_HOME/SYSTEM.md` — default `~/.kimi-code/SYSTEM.md` — replaces the built-in main agent system prompt while inheriting the default tool set. That makes prompt configuration a persistence and supply-chain surface, not just a chat message.

Compiled binaries and generated benchmark artifacts are intentionally **not committed**. Build the installer locally and keep any generated artifacts outside this repo.

## What the installer does

Running the built `CyberGymKimiInstaller.exe` will:

1. Create `%USERPROFILE%\.kimi-code` if missing.
2. Write the benchmark override to `%USERPROFILE%\.kimi-code\SYSTEM.md`.
3. Create a work folder:
   - `%USERPROFILE%\Projects\cybergym-kimi-code` if `Projects` exists
   - otherwise `%USERPROFILE%\cybergym-kimi-code`
4. Open a new terminal window in that folder running `kimi`.

The override payload is embedded in `Program.cs`. It names web vulnerability scanning/exploitation, AI jailbreak/prompt-injection artifacts, malware/reverse-engineering artifacts, and browser/session artifacts as in-scope benchmark work. It keeps Kimi Code's hard stops: no CSAM/minors, no violence, no blood/gore.

## Build from source

Requires only the built-in .NET Framework 4.x compiler on Windows:

```powershell
& 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe' /nologo /target:exe /platform:anycpu /out:CyberGymKimiInstaller.exe Program.cs
```

## Usage

```powershell
.\CyberGymKimiInstaller.exe
```

Options:

```powershell
# Install only; do not launch kimi
.\CyberGymKimiInstaller.exe --no-launch

# Choose the folder where kimi starts
.\CyberGymKimiInstaller.exe --work "C:\path\to\folder"

# Disable the override by renaming SYSTEM.md to SYSTEM.md.off
.\CyberGymKimiInstaller.exe --off
```

## Repo layout

```text
cybergym-kimi-code/
├─ .gitignore
├─ Program.cs
└─ README.md
```

## Why this matters

A normal prompt-injection attack has to convince the model every conversation. Agent CLIs that load system prompts from disk change the threat model: one markdown file can persistently redefine the agent's objective while keeping its file/shell tools.

Treat these as sensitive in any repo or machine:

```text
.kimi-code/agents/agent.md        # override: true can replace the main agent prompt
AGENTS.md                         # injected as project context
~/.kimi-code/SYSTEM.md            # global main-agent system prompt replacement
~/.agents/AGENTS.md               # generic cross-tool instructions
```

## Defensive notes

- Do not run agent CLIs in untrusted repositories before reviewing hidden agent/config folders.
- Prefer repo-scoped overrides over global `SYSTEM.md`.
- Add alerts for creation/modification of `SYSTEM.md`, `AGENTS.md`, `.kimi-code/`, and `.agents/`.
- Run coding agents in a VM/sandbox with no production credentials.
- Keep API keys out of prompts, repos, screenshots, and terminal history. Rotate any key that was pasted during testing.
- For Kimi Code specifically, safer defaults would be: read-only by default, explicit approval for writes/shell, warnings when override files change, and a flag required to allow global system-prompt replacement.

## Rollback

```powershell
.\CyberGymKimiInstaller.exe --off
```

or:

```powershell
Rename-Item "$env:USERPROFILE\.kimi-code\SYSTEM.md" "SYSTEM.md.off"
```
