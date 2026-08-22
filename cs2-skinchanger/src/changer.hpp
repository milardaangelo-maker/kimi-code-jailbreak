#pragma once

// Core skin-changer logic.
//
// Per tick:
//   1. Resolve the local player controller -> pawn -> weapon services.
//   2. Walk m_hMyWeapons, resolve each weapon handle to an entity.
//   3. For each weapon's embedded C_EconItemView:
//        - optionally swap m_iItemDefinitionIndex (knife model swap)
//        - set m_iItemID = UINT64_MAX so the client falls back to the local
//          attribute list instead of the replicated inventory item
//        - set m_iAccountID to the local Steam3 account id (StatTrak display)
//        - patch / append attributes in m_AttributeList:
//              6 = paint kit, 7 = pattern seed, 8 = wear, 80/81 = StatTrak
//   4. Optionally apply the same attribute patch to the pawn's gloves.
//
// Everything is cosmetic and local-only: the server replicates the real item
// to every other client, so nobody else sees the spoofed skin.

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config.hpp"
#include "memory.hpp"
#include "offsets.hpp"
#include "paintkits.hpp"

namespace attr {
constexpr uint16_t kPaintKit = 6;
constexpr uint16_t kPatternSeed = 7;
constexpr uint16_t kPaintWear = 8;
constexpr uint16_t kStatTrakKills = 80;
constexpr uint16_t kStatTrakType = 81;
constexpr uint16_t kStickerSlotBase = 113; // slot n id = 113 + 4n
} // namespace attr

struct SkinRule {
    uint16_t matchDefIndex = 0;  // weapon def index this rule targets
    bool hasPaintKit = false;
    int paintKit = 0;
    float wear = 0.0001f;
    int seed = 0;
    int statTrak = -1;           // -1 = leave alone
    int quality = -1;            // -1 = leave alone (9 = StatTrak quality)
    uint16_t swapToDefIndex = 0; // != 0 -> knife/model swap
    bool valid = false;
};

class Changer {
public:
    Changer(Memory& mem, const Offsets& off, uintptr_t clientBase)
        : mem_(mem), off_(off), client_(clientBase) {}

    void LoadRules(const Config& skins) {
        rules_.clear();
        knifeRule_ = SkinRule{};
        gloveRule_ = SkinRule{};
        gloveRuleSet_ = false;

        for (const auto& [key, value] : skins.All()) {
            // weapon.<name|id>.<field>
            if (key.rfind("weapon.", 0) == 0) {
                const auto dot = key.find('.', 7);
                if (dot == std::string::npos)
                    continue;
                const std::string which = key.substr(7, dot - 7);
                const std::string field = key.substr(dot + 1);

                uint16_t def = 0;
                if (!db::ResolveWeapon(which, def)) {
                    printf("[!] unknown weapon '%s', skipping\n", which.c_str());
                    continue;
                }
                SkinRule& rule = rules_[def];
                rule.matchDefIndex = def;
                rule.valid = true;
                ApplyField(rule, field, value);
            } else if (key.rfind("knife.", 0) == 0) {
                knifeRule_.valid = true;
                ApplyField(knifeRule_, key.substr(6), value);
            } else if (key.rfind("glove.", 0) == 0) {
                gloveRule_.valid = true;
                gloveRuleSet_ = true;
                ApplyField(gloveRule_, key.substr(6), value);
            }
        }

        // Knife rule defaults: swap both default knives to the requested model.
        if (knifeRule_.valid && knifeRule_.swapToDefIndex) {
            for (uint16_t def : {42, 59}) { // weapon_knife, weapon_knife_t
                SkinRule r = knifeRule_;
                r.matchDefIndex = def;
                rules_[def] = r;
            }
        }
        printf("[*] loaded %zu weapon rule(s)%s%s\n", rules_.size(),
               knifeRule_.valid ? " + knife swap" : "",
               gloveRuleSet_ ? " + gloves" : "");
    }

    // One application pass over the local inventory. Returns false when the
    // local player isn't available (menu, loading, etc.).
    bool Tick() {
        const uintptr_t controller = mem_.Read<uintptr_t>(client_ + off_.dwLocalPlayerController);
        if (!controller)
            return false;

        const uint64_t steamId = mem_.Read<uint64_t>(controller + off_.m_steamID);
        accountId_ = static_cast<uint32_t>(steamId & 0xFFFFFFFF);

        const uint32_t pawnHandle = mem_.Read<uint32_t>(controller + off_.m_hPawn);
        const uintptr_t pawn = ResolveHandle(pawnHandle);
        if (!pawn)
            return false;

        const uintptr_t weaponServices = mem_.Read<uintptr_t>(pawn + off_.m_pWeaponServices);
        if (!weaponServices)
            return false;

        const uintptr_t vecBase = weaponServices + off_.m_hMyWeapons;
        const int count = mem_.Read<int>(vecBase + off_.vec_CountRel);
        const uintptr_t data = mem_.Read<uintptr_t>(vecBase + off_.vec_DataRel);
        if (!data || count <= 0 || count > 64)
            return false;

        std::vector<uint32_t> handles(static_cast<size_t>(count));
        if (!mem_.ReadRaw(data, handles.data(), handles.size() * sizeof(uint32_t)))
            return false;

        for (uint32_t handle : handles) {
            const uintptr_t weapon = ResolveHandle(handle);
            if (weapon)
                ApplyToWeapon(weapon);
        }

        if (gloveRuleSet_ && off_.m_EconGloves)
            ApplyToItemView(pawn + off_.m_EconGloves, gloveRule_, /*allowSwap=*/false);

        return true;
    }

    // Call when leaving the server / changing config so weapons get re-stamped.
    void ResetState() { stamped_.clear(); }

private:
    static void ApplyField(SkinRule& rule, const std::string& field, const std::string& value) {
        if (field == "paintkit") {
            int pk = 0;
            if (db::ResolvePaintKit(value, pk)) {
                rule.paintKit = pk;
                rule.hasPaintKit = true;
            } else {
                printf("[!] unknown paint kit '%s'\n", value.c_str());
            }
        } else if (field == "wear") {
            rule.wear = std::stof(value);
        } else if (field == "seed") {
            rule.seed = std::stoi(value);
        } else if (field == "stattrak") {
            rule.statTrak = std::stoi(value);
            rule.quality = 9;
        } else if (field == "quality") {
            rule.quality = std::stoi(value);
        } else if (field == "defindex") {
            uint16_t def = 0;
            if (db::ResolveWeapon(value, def))
                rule.swapToDefIndex = def;
        }
    }

    uintptr_t ResolveHandle(uint32_t handle) const {
        if (!handle || handle == 0xFFFFFFFF)
            return 0;
        const uint32_t idx = handle & 0x7FFF;
        const uintptr_t entityList = mem_.Read<uintptr_t>(client_ + off_.dwEntityList);
        if (!entityList)
            return 0;
        const uintptr_t chunk =
            mem_.Read<uintptr_t>(entityList + off_.entity_ChunkBase +
                                 0x8 * (idx >> off_.entity_ChunkShift));
        if (!chunk)
            return 0;
        return mem_.Read<uintptr_t>(chunk + off_.entity_IdentityStride * (idx & off_.entity_IndexMask));
    }

    void ApplyToWeapon(uintptr_t weapon) {
        const uintptr_t itemView = weapon + off_.m_AttributeManager + off_.m_Item;
        const uint16_t defIndex = mem_.Read<uint16_t>(itemView + off_.m_iItemDefinitionIndex);

        auto it = rules_.find(defIndex);
        if (it == rules_.end())
            return;

        ApplyToItemView(itemView, it->second, /*allowSwap=*/true);
    }

    void ApplyToItemView(uintptr_t itemView, const SkinRule& rule, bool allowSwap) {
        if (!rule.valid)
            return;

        // One-time stamps per item-view address. m_iItemID = -1 makes the
        // client ignore the replicated inventory item and use the local
        // attribute list; account id makes StatTrak counters render.
        if (stamped_.find(itemView) == stamped_.end()) {
            mem_.Write<uint64_t>(itemView + off_.m_iItemID, 0xFFFFFFFFFFFFFFFFULL);
            if (accountId_)
                mem_.Write<uint32_t>(itemView + off_.m_iAccountID, accountId_);
            if (off_.m_bInitialized)
                mem_.Write<uint8_t>(itemView + off_.m_bInitialized, 1);
            stamped_.insert(itemView);
        }

        if (allowSwap && rule.swapToDefIndex)
            mem_.Write<uint16_t>(itemView + off_.m_iItemDefinitionIndex, rule.swapToDefIndex);

        if (rule.quality >= 0 && off_.m_iEntityQuality)
            mem_.Write<int32_t>(itemView + off_.m_iEntityQuality, rule.quality);

        const uintptr_t attrList = itemView + off_.m_AttributeList + off_.m_Attributes;

        if (rule.hasPaintKit) {
            SetAttributeInt(attrList, itemView, attr::kPaintKit, static_cast<uint32_t>(rule.paintKit));
            SetAttributeInt(attrList, itemView, attr::kPatternSeed, static_cast<uint32_t>(rule.seed));
            SetAttributeFloat(attrList, itemView, attr::kPaintWear, rule.wear);
        }
        if (rule.statTrak >= 0) {
            SetAttributeInt(attrList, itemView, attr::kStatTrakKills, static_cast<uint32_t>(rule.statTrak));
            SetAttributeInt(attrList, itemView, attr::kStatTrakType, 0);
        }
    }

    // Finds an attribute in the list; returns its element address or 0.
    uintptr_t FindAttribute(uintptr_t attrList, int count, uintptr_t data, uint16_t defIndex) const {
        for (int i = 0; i < count; ++i) {
            const uintptr_t elem = data + static_cast<uintptr_t>(i) * off_.attr_Stride;
            const uint16_t id = mem_.Read<uint16_t>(elem + off_.attr_DefIndex);
            if (id == defIndex)
                return elem;
        }
        (void)attrList;
        return 0;
    }

    // Writes an integer attribute, growing the vector in remote memory when
    // the attribute doesn't exist yet (vanilla weapons have empty lists).
    void SetAttributeInt(uintptr_t attrList, uintptr_t itemView, uint16_t defIndex, uint32_t value) {
        uintptr_t elem = EnsureAttribute(attrList, itemView, defIndex);
        if (elem)
            mem_.Write<uint32_t>(elem + off_.attr_Value, value);
    }

    void SetAttributeFloat(uintptr_t attrList, uintptr_t itemView, uint16_t defIndex, float value) {
        uintptr_t elem = EnsureAttribute(attrList, itemView, defIndex);
        if (elem)
            mem_.Write<float>(elem + off_.attr_FloatValue, value);
    }

    uintptr_t EnsureAttribute(uintptr_t attrList, uintptr_t itemView, uint16_t defIndex) {
        const int count = mem_.Read<int>(attrList + off_.vec_CountRel);
        const uintptr_t data = mem_.Read<uintptr_t>(attrList + off_.vec_DataRel);

        if (data && count > 0 && count < 128) {
            if (uintptr_t elem = FindAttribute(attrList, count, data, defIndex))
                return elem;
        }

        // Grow path: allocate a new remote buffer, copy existing elements,
        // append ours, then repoint the vector. The old game-owned buffer is
        // left untouched; the game frees/ignores per its own logic.
        const int newCount = (count > 0 && count < 128) ? count + 1 : 1;
        const size_t bytes = static_cast<size_t>(newCount) * off_.attr_Stride;

        const uintptr_t fresh = mem_.Alloc(bytes);
        if (!fresh)
            return 0;

        if (data && count > 0 && count < 128) {
            std::vector<uint8_t> copy(bytes, 0);
            if (mem_.ReadRaw(data, copy.data(), static_cast<size_t>(count) * off_.attr_Stride))
                mem_.WriteRaw(fresh, copy.data(), copy.size());
        } else {
            std::vector<uint8_t> zero(bytes, 0);
            mem_.WriteRaw(fresh, zero.data(), zero.size());
        }

        const uintptr_t elem = fresh + static_cast<uintptr_t>(newCount - 1) * off_.attr_Stride;
        mem_.Write<uint16_t>(elem + off_.attr_DefIndex, defIndex);

        mem_.Write<uintptr_t>(attrList + off_.vec_DataRel, fresh);
        mem_.Write<int>(attrList + off_.vec_CountRel, newCount);

        remoteAllocs_[itemView].push_back(fresh);
        return elem;
    }

    Memory& mem_;
    const Offsets& off_;
    uintptr_t client_ = 0;
    uint32_t accountId_ = 0;

    std::unordered_map<uint16_t, SkinRule> rules_;
    SkinRule knifeRule_{};
    SkinRule gloveRule_{};
    bool gloveRuleSet_ = false;

    std::unordered_set<uintptr_t> stamped_;
    std::unordered_map<uintptr_t, std::vector<uintptr_t>> remoteAllocs_;
};
