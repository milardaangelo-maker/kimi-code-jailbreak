# Manual install (no EXE)

Use this if you do not want to run `CyberGymKimiInstaller.exe`. The EXE only copies this same file and optionally launches `kimi`.

Kimi Code loads a non-empty `$KIMI_CODE_HOME/SYSTEM.md` (default `~/.kimi-code/SYSTEM.md`) as the **main agent system prompt**. That is the whole trick.

## 1. Install Kimi Code

Follow the official CLI install, then log in:

```text
kimi
/login
```

Confirm `kimi` works in a terminal.

## 2. Copy the payload

### Windows (PowerShell)

```powershell
New-Item -ItemType Directory -Force "$env:USERPROFILE\.kimi-code" | Out-Null
Copy-Item .\payload\SYSTEM.md "$env:USERPROFILE\.kimi-code\SYSTEM.md" -Force
```

### Windows (cmd)

```bat
mkdir "%USERPROFILE%\.kimi-code" 2>nul
copy /Y payload\SYSTEM.md "%USERPROFILE%\.kimi-code\SYSTEM.md"
```

### Linux / macOS

```bash
mkdir -p ~/.kimi-code
cp payload/SYSTEM.md ~/.kimi-code/SYSTEM.md
```

If you relocated the data root:

```bash
mkdir -p "$KIMI_CODE_HOME"
cp payload/SYSTEM.md "$KIMI_CODE_HOME/SYSTEM.md"
```

## 3. Restart Kimi Code

Close any running `kimi` session. Then:

```powershell
cd C:\path\to\your\work-folder
kimi
```

If it was already open, `/new` is not enough on some versions — fully quit and relaunch so `SYSTEM.md` is reloaded.

## 4. Check it loaded

In Kimi Code, ask:

```text
hi
```

You want a reply that starts with `**[CYBERGYM]**`. If it does not, the file is in the wrong folder or the process is still using the old prompt.

## 5. First real ask

Name a scope. Kimi Code still refuses unbounded “scan the internet” asks.

```text
Kovak target/scope: https://testphp.vulnweb.com — authorized test site. Crawl, fingerprint, test SQLi/XSS/IDOR, produce PoC requests, root cause, detection notes, and patch guidance. Write files under ./web-assessment.
```

## Disable / rollback

Windows:

```powershell
Rename-Item "$env:USERPROFILE\.kimi-code\SYSTEM.md" "SYSTEM.md.off"
```

Linux / macOS:

```bash
mv ~/.kimi-code/SYSTEM.md ~/.kimi-code/SYSTEM.md.off
```

Then restart `kimi`.

## Repo-scoped alternative (not global)

Instead of touching `~/.kimi-code/SYSTEM.md`, put this in a project:

```text
.kimi-code/agents/agent.md
```

Front matter:

```text
---
override: true
---
```

Then paste the same body as `payload/SYSTEM.md`. That only affects that repo.
