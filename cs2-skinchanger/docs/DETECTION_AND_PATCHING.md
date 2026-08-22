# Root Cause, Detection & Patch Guidance

## Root cause

The vulnerability class is **client-authoritative cosmetic rendering state**:

1. The client ships every skin's assets and the full finish table
   (`items_game.txt` compiled in). Ownership is enforced only by which
   *attributes* the server replicates — the rendering capability is
   unconditional.
2. The composite-material pipeline keys off the **local** `C_EconItemView`
   with no integrity check: no checksum against the server-replicated item,
   no signature, no read-only protection of the attribute list.
3. `m_iItemID = -1` provides a built-in fallback path that treats the local
   attribute list as authoritative — a legitimate engine feature (item
   preview, inspect-in-browser, demo playback) that doubles as the spoof
   primitive.

In short: the renderer trusts writable, unvalidated memory for cosmetic
decisions, and the engine ships a supported code path that prefers local
attributes over replicated ones.

## Why server-side validation alone cannot fix it

The server already is authoritative for what *other* clients see — that is
why changers are local-only. The residual "vulnerability" is the local
client's own rendering, and a remote server cannot distinguish "client
rendering paint kit 420 because the user owns it" from "...because memory
was patched" without client-side attestation. Closing the gap requires
client-integrity measures, i.e. anti-cheat, below.

## Patch / mitigation guidance (developer side)

1. **Validate the fallback path.** When `m_iItemID` is invalid/-1, restrict
   local attributes to contexts that legitimately need them (inspect/preview
   UI) and refuse them for in-world equipped entities. A one-line state check
   kills the `m_iItemID = -1` primitive for live weapons.
2. **Checksum econ views against replicated state.** On attribute change,
   compare the local list to the last server-replicated snapshot for that
   item id; on mismatch, restore and optionally report. Cheap because
   attribute lists are tiny.
3. **Harden storage.** Keep the live attribute list in obfuscated/encoded
   form (XOR with a per-entity nonce, decoded only inside the material-key
   builder). Raises the bar from "write two integers" to per-build reverse
   engineering, and breaks generic external tools each update.
4. **Integrity attestation.** Periodic client self-checks (module + critical
   data hashes) reported to the server; already the direction of VAC/Trusted
   Mode — extending coverage to econ schema data closes this specific hole.

## Detection guidance (anti-cheat side)

- **Handle auditing:** alert on non-allowlisted processes holding
  `PROCESS_VM_WRITE` handles to the game; kernel side, `ObRegisterCallbacks`
  can strip write rights at open time.
- **Allocation scanning:** flag anonymous RW private pages in the game's
  address space that no game module allocated (catches the `VirtualAllocEx`
  grow path).
- **Pointer-range heuristics:** walk equipped weapons' attribute lists; a
  `m_Attributes` data pointer outside the game allocator's arenas is a
  high-confidence repoint indicator.
- **State consistency:** `m_iItemID == UINT64_MAX` on an equipped,
  in-world weapon; quality 9 without attribute 80; paint kit ids not valid
  for the weapon's def index. All cheap periodic scans.
- **Behavioral:** per-frame `WriteProcessMemory` cadence from a single
  external process is itself a signature even when individual writes vary.

## Guidance for server admins / tournament ops

- Changers are **invisible in demos, GOTV, and spectator view** — absence of
  evidence in a demo is not evidence of absence.
- Enforce at the client-integrity layer (AC client with handle/memory
  scanning) rather than gameplay review.
- Stream/screenshare checks catch lazy variants (tool window visible) but not
  the mechanism itself; treat as supplementary.

## For researchers reproducing this PoC

- Use an isolated machine/account; expect VAC to eventually flag the write
  pattern. Never run against inventory you value.
- The PoC's config-driven offsets make it a useful harness for studying
  schema churn across updates — diff successive `cs2-dumper` outputs to see
  exactly which structures Valve touches per patch.
