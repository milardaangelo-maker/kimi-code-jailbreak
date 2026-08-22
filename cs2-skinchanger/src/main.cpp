// cs2-skinchanger - external CS2 skin/knife/glove changer PoC.
//
// Usage:
//   1. Regenerate config/offsets.cfg for the current CS2 build
//      (see tools/cs2dumper_to_cfg.py).
//   2. Edit config/skins.cfg to taste.
//   3. Launch CS2, then run this tool (same user; no injection performed).
//
// Console keys: [R] reload skins.cfg, [Q] quit.

#include <conio.h>
#include <windows.h>

#include <cstdio>
#include <string>

#include "changer.hpp"
#include "config.hpp"
#include "memory.hpp"
#include "offsets.hpp"

namespace {

std::string ExeDir() {
    char buf[MAX_PATH]{};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path(buf);
    const auto slash = path.find_last_of("\\/");
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

} // namespace

int main() {
    printf("cs2-skinchanger PoC (external, cosmetic, local-only)\n");
    printf("----------------------------------------------------\n");

    const std::string dir = ExeDir();
    const std::string offsetsPath = dir + "\\config\\offsets.cfg";
    const std::string skinsPath = dir + "\\config\\skins.cfg";

    Config offsetsCfg;
    if (!offsetsCfg.Load(offsetsPath)) {
        printf("[!] cannot open %s\n", offsetsPath.c_str());
        printf("    generate it with tools/cs2dumper_to_cfg.py first.\n");
        return 1;
    }

    Offsets off;
    std::string missing;
    if (!off.LoadFrom(offsetsCfg, missing)) {
        printf("[!] offsets.cfg is missing required key: %s\n", missing.c_str());
        return 1;
    }

    Config skinsCfg;
    if (!skinsCfg.Load(skinsPath))
        printf("[!] %s not found, running with no rules (harmless)\n", skinsPath.c_str());

    printf("[*] waiting for cs2.exe");
    Memory mem;
    while (!mem.Attach(L"cs2.exe")) {
        printf(".");
        Sleep(1000);
    }
    printf("\n[*] attached to pid %lu\n", mem.Pid());

    uintptr_t client = 0;
    while (!(client = mem.ModuleBase(L"client.dll"))) {
        printf("[*] waiting for client.dll ...\n");
        Sleep(1000);
    }
    printf("[*] client.dll @ 0x%p\n", reinterpret_cast<void*>(client));

    Changer changer(mem, off, client);
    changer.LoadRules(skinsCfg);

    bool wasInGame = false;
    for (;;) {
        if (_kbhit()) {
            const int c = _getch();
            if (c == 'q' || c == 'Q')
                break;
            if (c == 'r' || c == 'R') {
                Config fresh;
                if (fresh.Load(skinsPath)) {
                    changer.LoadRules(fresh);
                    changer.ResetState();
                    printf("[*] skins.cfg reloaded\n");
                }
            }
        }

        const bool inGame = changer.Tick();
        if (inGame && !wasInGame)
            printf("[*] local player found, applying skins\n");
        if (!inGame && wasInGame) {
            printf("[*] local player lost (menu/disconnect), pausing\n");
            changer.ResetState();
        }
        wasInGame = inGame;

        Sleep(inGame ? 5 : 500);
    }

    printf("[*] bye\n");
    return 0;
}
