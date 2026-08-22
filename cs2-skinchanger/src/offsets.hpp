#pragma once

// Typed view over offsets.cfg. Every value is runtime-configurable because
// CS2 recompiles shift these constantly. Regenerate with a schema dumper
// (e.g. a2x/cs2-dumper) + tools/cs2dumper_to_cfg.py after each game update.

#include <cstdint>
#include <string>

#include "config.hpp"

struct Offsets {
    // client.dll globals
    uintptr_t dwEntityList = 0;
    uintptr_t dwLocalPlayerController = 0;
    uintptr_t dwLocalPlayerPawn = 0; // optional fast path

    // schema: controller / pawn
    uintptr_t m_hPawn = 0;             // CBasePlayerController -> pawn handle
    uintptr_t m_steamID = 0;           // CBasePlayerController -> uint64 SteamID
    uintptr_t m_pWeaponServices = 0;   // C_CSPlayerPawnBase -> CPlayer_WeaponServices*
    uintptr_t m_EconGloves = 0;        // C_CSPlayerPawn -> embedded C_EconItemView

    // schema: weapon services
    uintptr_t m_hMyWeapons = 0;        // CPlayer_WeaponServices -> C_NetworkUtlVectorBase<CHandle>
    uintptr_t m_hActiveWeapon = 0;     // CPlayer_WeaponServices -> CHandle

    // schema: econ
    uintptr_t m_AttributeManager = 0;  // C_EconEntity -> embedded C_AttributeContainer
    uintptr_t m_Item = 0;              // C_AttributeContainer -> embedded C_EconItemView
    uintptr_t m_iItemDefinitionIndex = 0;
    uintptr_t m_iEntityQuality = 0;
    uintptr_t m_iItemID = 0;
    uintptr_t m_iAccountID = 0;
    uintptr_t m_bInitialized = 0;
    uintptr_t m_AttributeList = 0;     // C_EconItemView -> embedded CAttributeList
    uintptr_t m_Attributes = 0;        // CAttributeList -> CUtlVectorEmbeddedNetworkVar

    // schema: attribute element layout (CEconItemAttribute)
    uintptr_t attr_DefIndex = 0;       // uint16 attribute definition index
    uintptr_t attr_Value = 0;          // uint32 integer value
    uintptr_t attr_FloatValue = 0;     // float value
    uintptr_t attr_Stride = 0;         // sizeof(CEconItemAttribute)

    // C_NetworkUtlVectorBase layout, relative to the vector base
    uintptr_t vec_CountRel = 0;
    uintptr_t vec_DataRel = 0;

    // entity handle -> pointer resolution
    uint32_t entity_ChunkBase = 0x10;    // entityList + chunkBase + 8 * (idx >> shift)
    uint32_t entity_ChunkShift = 9;
    uint32_t entity_IndexMask = 0x1FF;
    uint32_t entity_IdentityStride = 0x78;

    bool LoadFrom(const Config& cfg, std::string& missing) {
        auto need = [&](const char* key, uintptr_t& out) {
            if (!cfg.Has(key)) {
                missing = key;
                return false;
            }
            out = static_cast<uintptr_t>(cfg.GetU64(key));
            return true;
        };

        // Required globals + schema fields.
        if (!need("client.dwEntityList", dwEntityList)) return false;
        if (!need("client.dwLocalPlayerController", dwLocalPlayerController)) return false;
        dwLocalPlayerPawn = static_cast<uintptr_t>(cfg.GetU64("client.dwLocalPlayerPawn", 0));

        if (!need("schema.CBasePlayerController.m_hPawn", m_hPawn)) return false;
        if (!need("schema.CBasePlayerController.m_steamID", m_steamID)) return false;
        if (!need("schema.C_CSPlayerPawnBase.m_pWeaponServices", m_pWeaponServices)) return false;
        m_EconGloves = static_cast<uintptr_t>(cfg.GetU64("schema.C_CSPlayerPawn.m_EconGloves", 0));

        if (!need("schema.CPlayer_WeaponServices.m_hMyWeapons", m_hMyWeapons)) return false;
        m_hActiveWeapon = static_cast<uintptr_t>(cfg.GetU64("schema.CPlayer_WeaponServices.m_hActiveWeapon", 0));

        if (!need("schema.C_EconEntity.m_AttributeManager", m_AttributeManager)) return false;
        if (!need("schema.C_AttributeContainer.m_Item", m_Item)) return false;
        if (!need("schema.C_EconItemView.m_iItemDefinitionIndex", m_iItemDefinitionIndex)) return false;
        m_iEntityQuality = static_cast<uintptr_t>(cfg.GetU64("schema.C_EconItemView.m_iEntityQuality", 0));
        if (!need("schema.C_EconItemView.m_iItemID", m_iItemID)) return false;
        if (!need("schema.C_EconItemView.m_iAccountID", m_iAccountID)) return false;
        m_bInitialized = static_cast<uintptr_t>(cfg.GetU64("schema.C_EconItemView.m_bInitialized", 0));
        if (!need("schema.C_EconItemView.m_AttributeList", m_AttributeList)) return false;
        if (!need("schema.CAttributeList.m_Attributes", m_Attributes)) return false;

        if (!need("schema.CEconItemAttribute.m_iAttributeDefinitionIndex", attr_DefIndex)) return false;
        if (!need("schema.CEconItemAttribute.m_nValue", attr_Value)) return false;
        if (!need("schema.CEconItemAttribute.m_flValue", attr_FloatValue)) return false;
        if (!need("layout.attr.stride", attr_Stride)) return false;

        vec_CountRel = static_cast<uintptr_t>(cfg.GetU64("layout.utlvector.count_rel", 0x0));
        vec_DataRel = static_cast<uintptr_t>(cfg.GetU64("layout.utlvector.data_rel", 0x8));

        entity_ChunkBase = cfg.GetInt("layout.entity.chunk_base", 0x10);
        entity_ChunkShift = cfg.GetInt("layout.entity.chunk_shift", 9);
        entity_IndexMask = cfg.GetInt("layout.entity.index_mask", 0x1FF);
        entity_IdentityStride = cfg.GetInt("layout.entity.identity_stride", 0x78);
        return true;
    }
};
