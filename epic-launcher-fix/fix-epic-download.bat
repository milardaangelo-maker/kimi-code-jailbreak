@echo off
setlocal
:: ============================================================
::  fix-epic-download.bat
::  One-click fix for Epic Games Launcher downloads stuck at
::  "0 seconds remaining" / 0 MB/s (Fortnite and friends).
::
::  What it does:
::    1. Closes the launcher (and its web helpers)
::    2. Clears the launcher webcache (most common culprit)
::    3. Applies the Engine.ini chunk-download fix
::    4. Flushes the DNS cache
::    5. Resets any broken system-level proxy
::    6. Reports free disk space, then relaunches the launcher
::
::  Usage: right-click this file -> "Run as administrator"
:: ============================================================

net session >nul 2>&1
if not %errorlevel%==0 (
    echo [!] Right-click this file and choose "Run as administrator".
    pause
    exit /b 1
)

echo [1/6] Closing Epic Games Launcher...
taskkill /f /im EpicGamesLauncher.exe >nul 2>&1
taskkill /f /im EpicWebHelper.exe >nul 2>&1
timeout /t 2 /nobreak >nul

echo [2/6] Clearing launcher webcache...
if exist "%localappdata%\EpicGamesLauncher\Saved" (
    pushd "%localappdata%\EpicGamesLauncher\Saved"
    for /d %%D in (webcache*) do rd /s /q "%%D" >nul 2>&1
    popd
)

echo [3/6] Applying Engine.ini download fix...
set "CFGDIR=%localappdata%\EpicGamesLauncher\Saved\Config\Windows"
if not exist "%CFGDIR%" mkdir "%CFGDIR%"
set "INI=%CFGDIR%\Engine.ini"
set "PATCHED=0"
if exist "%INI%" findstr /c:"[Portal.BuildPatch]" "%INI%" >nul 2>&1 && set "PATCHED=1"
if "%PATCHED%"=="1" (
    echo     Engine.ini already patched, skipping.
) else (
    if exist "%INI%" copy /y "%INI%" "%INI%.bak" >nul
    (
        echo(
        echo [HTTP]
        echo HttpTimeout=10
        echo HttpConnectionTimeout=10
        echo HttpReceiveTimeout=10
        echo HttpSendTimeout=10
        echo(
        echo [Portal.BuildPatch]
        echo ChunkDownloads=3
        echo ChunkRetries=20
        echo RetryTime=0.5
    ) >> "%INI%"
    echo     fix written to Engine.ini (backup: Engine.ini.bak)
)

echo [4/6] Flushing DNS cache...
ipconfig /flushdns >nul

echo [5/6] Resetting system proxy (harmless if none is set)...
netsh winhttp reset proxy >nul

echo [6/6] Free disk space (Fortnite needs roughly 40 GB+ free):
for %%L in (C D E) do (
    if exist "%%L:\" (
        for /f "tokens=*" %%F in ('fsutil volume diskfree %%L: 2^>nul ^| findstr /i "Total free"') do echo     %%L:  %%F
    )
)

echo.
echo Relaunching Epic Games Launcher...
set "EPIC64=%ProgramFiles(x86)%\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe"
set "EPIC32=%ProgramFiles(x86)%\Epic Games\Launcher\Portal\Binaries\Win32\EpicGamesLauncher.exe"
if exist "%EPIC64%" (
    start "" "%EPIC64%"
) else if exist "%EPIC32%" (
    start "" "%EPIC32%"
) else (
    echo [!] Launcher not found in the default path - open it manually.
)

echo.
echo Done. Resume the download in the launcher.
echo.
echo If it STILL shows 0 MB/s: open Task Manager -^> Performance -^> Disk.
echo High disk activity means it is unpacking chunks, not stuck - let it
echo finish. If the drive is nearly full, free up space or move the
echo install to another drive (Launcher -^> Settings -^> Install Location).
pause
