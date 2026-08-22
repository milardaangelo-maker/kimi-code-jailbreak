# cs2-skinchanger

External skin / knife / glove changer PoC for Counter-Strike 2 (Windows).
Pure `ReadProcessMemory`/`WriteProcessMemory` — no injection, no hooks, no
threads created in the target.

> Security-research benchmark artifact. Modifying game memory violates the
> Steam/Valve ToS and is detectable by VAC. Run only on throwaway
> accounts/machines. See `docs/EXPLOITABILITY.md`.

## Layout

```
src/
  main.cpp        entry point, attach loop, hotkeys ([R]eload, [Q]uit)
  memory.hpp      OpenProcess + RPM/WPM + VirtualAllocEx wrapper
  changer.hpp     entity walk + econ attribute patching (core logic)
  offsets.hpp     typed view over offsets.cfg (zero hardcoded offsets)
  config.hpp      key=value config parser
  paintkits.hpp   weapon def-index + paint-kit name tables
config/
  offsets.cfg     per-build offsets (TEMPLATE - regenerate before use!)
  skins.cfg       what to apply (per-weapon paint kit / wear / seed / StatTrak)
tools/
  cs2dumper_to_cfg.py   converts a2x/cs2-dumper JSON -> offsets.cfg
docs/
  TECHNICAL_ANALYSIS.md    how the econ system + spoof work, in depth
  EXPLOITABILITY.md        impact + detection-surface assessment
  DETECTION_AND_PATCHING.md  root cause, Valve-side fixes, AC detection
```

## Build

Windows + MSVC:

```bat
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Or cross-compile from Linux with MinGW:

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x64.cmake
cmake --build build
```

## Run

1. **Regenerate offsets for the current CS2 build** — the shipped
   `config/offsets.cfg` is a zeroed template and will not work as-is:

   ```bat
   cs2-dumper.exe            :: https://github.com/a2x/cs2-dumper
   python tools\cs2dumper_to_cfg.py output\offsets.json output\client_dll.json -o config\offsets.cfg
   ```

   Then verify the `layout.*` keys by hand (they are structural and not
   emitted by schema dumpers).

2. Edit `config/skins.cfg` (ships with Asiimov AK, Dragon Lore, Howl, Blaze,
   Karambit Doppler Sapphire as examples).

3. Launch CS2, then run `cs2-skinchanger.exe` as the same user. Press
   `R` to hot-reload `skins.cfg`, `Q` to quit.

## How it works (60-second version)

Every weapon entity embeds a `C_EconItemView` whose attribute list
(`paint kit = attr 6`, `seed = 7`, `wear = 8`, `StatTrak = 80/81`) feeds the
client's composite-material renderer. The tool walks

`local controller -> pawn -> weapon services -> m_hMyWeapons -> each weapon`,

sets `m_iItemID = -1` so the local attribute list wins over the replicated
inventory item, stamps the owner's account id for StatTrak, and patches the
attributes — growing the vector in remote memory when the weapon has none.
Knife swap is a `m_iItemDefinitionIndex` rewrite (42/59 -> 507 etc.).
Full detail: `docs/TECHNICAL_ANALYSIS.md`.

Everything is **local and cosmetic**: the server replicates the real item to
everyone else, so teammates, demos, and GOTV never see the spoof.
