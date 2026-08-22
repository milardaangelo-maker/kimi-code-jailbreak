# CS2 Skin Changer — Technical Analysis

## 1. What a skin changer actually is

A skin changer is a **client-side cosmetic spoof**. It rewrites the economy
attributes of weapon entities *in the local game client's memory* so the
rendering pipeline draws a different finish, wear, pattern, StatTrak counter,
or knife model than the item the player actually owns.

It is not a server exploit. The game server holds the authoritative inventory
(via Steam's item servers) and replicates the *real* item to every other
client. Only the local machine is deceived, which is why changers are
invisible to teammates, spectators, demos, and overwatch-style review.

## 2. The Source 2 economy object model

CS2 (Source 2) represents every weapon in the world as an entity in the
client's entity list. The chain that matters:

```
CGameEntitySystem (client.dll + dwEntityList)
  └─ entity identity chunks (512 identities per chunk)
       └─ C_CSWeaponBase                       one per weapon in inventory
            └─ m_AttributeManager              C_AttributeContainer (embedded)
                 └─ m_Item                     C_EconItemView (embedded)
                      ├─ m_iItemDefinitionIndex   uint16  weapon/knife type
                      ├─ m_iItemID                uint64  inventory item id
                      ├─ m_iAccountID             uint32  owner's Steam3 id
                      ├─ m_iEntityQuality         int32   9 = StatTrak
                      ├─ m_bInitialized           bool
                      └─ m_AttributeList          CAttributeList (embedded)
                           └─ m_Attributes        CUtlVectorEmbeddedNetworkVar<CEconItemAttribute>
                                └─ elements: { uint16 defIndex; uint32 nValue;
                                               float flValue; char* szValue; }
```

Cosmetic state lives in the attribute list as `(attribute definition index,
value)` pairs. The stable, decade-old IDs inherited from the CS:GO item
schema:

| ID  | Meaning                    | Value field          |
|-----|----------------------------|----------------------|
| 6   | paint kit (the skin)       | nValue = kit id      |
| 7   | pattern seed               | nValue (0–1000)      |
| 8   | paint wear ("float")       | flValue (0.00–1.00)  |
| 80  | StatTrak kill counter      | nValue               |
| 81  | StatTrak score type        | nValue (0 = kills)   |
| 113+4n | sticker slot n id       | nValue = sticker kit |
| 114+4n | sticker slot n wear     | flValue              |

The **paint kit id is just an index into the finish table** compiled into the
game (from `items_game.txt`). Every client already ships every skin's
textures/materials — ownership only gates which attributes the server hands
you. That is the entire reason client-side spoofing is possible.

## 3. Rendering path: why the spoof works

When the engine draws a weapon it builds a **composite material**: it reads
the weapon's `C_EconItemView`, hashes `(defIndex, paintKit, wear, seed,
stickers)` into a material key, and generates (or reuses) the layered
texture. Two properties make the changer viable:

1. **The material key is derived from the local `C_EconItemView`.** If the
   attribute list says "paint kit 420, wear 0.0001", the client happily
   renders a Doppler Sapphire. There is no signature, checksum, or server
   round-trip on this path.
2. **`m_iItemID = UINT64_MAX` detaches the view from the replicated item.**
   The networked item id normally links the view to the real inventory entry
   (whose attributes the server controls and would overwrite ours). Setting
   the id to `-1` pushes the client onto the "uninitialized/fallback" path
   where the locally-written attribute list is authoritative for rendering.

`m_iAccountID` is set to the local player's Steam3 account id
(`m_steamID & 0xFFFFFFFF` from `CBasePlayerController`) so owner-gated
features — the StatTrak LED counter — render as "ours".

## 4. Entity and handle resolution

Entities are referenced by 32-bit handles, not pointers. Resolution:

```
idx        = handle & 0x7FFF
chunk      = *(entityList + chunk_base + 8 * (idx >> 9))   // 512 per chunk
entity     = *(chunk + identity_stride * (idx & 0x1FF))
```

`identity_stride` (historically `0x70`/`0x78`) and `chunk_base` shift between
builds, which is why they are config keys, not constants.

The walk to the weapons:

```
controller = *(client.dll + dwLocalPlayerController)
pawn       = resolve(*(controller + m_hPawn))
services   = *(pawn + m_pWeaponServices)
count      = *(services + m_hMyWeapons + count_rel)
data       = *(services + m_hMyWeapons + data_rel)
for i in 0..count: weapon = resolve(*(data + 4*i))
```

`m_hMyWeapons` is a `C_NetworkUtlVectorBase<CHandle<C_CSWeaponBase>>` — a
slim networked vector `{ count; pad; T* data; }`. The exact relative offsets
of `count`/`data` are build-dependent and configurable.

## 5. Writing the attributes

Two cases per attribute:

- **Patch in place.** The weapon already has the attribute (e.g. it has a
  real skin): locate the element by scanning `defIndex`, overwrite
  `nValue`/`flValue`. Zero allocation, minimal write footprint.
- **Grow the vector.** Vanilla weapons have an empty list. `CUtlVector`
  growth would normally call the game allocator; externally we instead
  `VirtualAllocEx` a scratch page in the target, copy the old elements,
  append ours, and repoint `{ data, count }`. The old buffer stays valid
  (game-owned), so nothing the engine still references is freed under it.

Trade-off: each grow leaks a few hundred remote bytes per weapon instance and
leaves an anonymous RW page in the target — both are detection surface, see
`DETECTION_AND_PATCHING.md`.

## 6. Knife model swap

Changing `m_iItemDefinitionIndex` on the default knife (`weapon_knife` = 42 /
`weapon_knife_t` = 59) to e.g. Karambit (507) changes which model/animation
set the client binds to the weapon. Practical notes:

- The new model must be precached. All knife models ship with the client and
  are typically precached on map load; if a swap renders as an error model,
  re-deploy (switch weapons) to force re-binding.
- Attack animations/sounds follow the new definition index client-side.
  Server-side hit registration is unchanged (knife reach is server logic).

## 7. Gloves

Gloves are a wearable econ item on the pawn: `C_CSPlayerPawn::m_EconGloves`
is an embedded `C_EconItemView`, so the identical attribute patch applies.
Model swap of gloves additionally requires touching the wearable's definition
index and re-creating the wearable entity — this PoC patches glove
*attributes* only.

## 8. Update cadence and offset management

Every CS2 update recompiles `client.dll`; schema offsets move. The workflow:

1. Run a schema dumper against the updated game (e.g. `a2x/cs2-dumper`).
2. `tools/cs2dumper_to_cfg.py output/offsets.json output/client_dll.json -o config/offsets.cfg`
3. Sanity-check `layout.*` keys (vector layout, entity stride) by hand —
   these are structural, not schema-networked, so dumpers don't emit them.

The PoC deliberately contains **zero hardcoded offsets**; `config/offsets.cfg`
ships zeroed as a template.

## 9. Why external (RPM/WPM) instead of internal (DLL injection)

| | External (this PoC) | Internal |
|---|---|---|
| Mechanism | OpenProcess + RPM/WPM | Inject DLL, hook game functions |
| Trusted Mode | no module load, nothing to block | unsigned DLLs blocked by Trusted Launch |
| Stability | offsets only | offsets + vtable hooks + SDK churn |
| Detection | handle + WPM heuristics | module scans, hook scans, handle scans |
| Feature ceiling | attribute writes | full engine hooks (force update events, sticker preview, etc.) |

For a benchmark PoC the external design is the minimal sufficient mechanism:
everything the renderer needs is reachable through plain memory writes.
