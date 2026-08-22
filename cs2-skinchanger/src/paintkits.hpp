#pragma once

// Curated paint-kit and item-definition tables. The authoritative source is
// the game's items_game.txt (or community dumps of it); this is a convenience
// subset of well-known, decade-stable IDs so skins.cfg can use names instead
// of raw numbers. Verify against a current dump if a value looks wrong.

#include <cstdint>
#include <string>
#include <unordered_map>

namespace db {

// Weapon item definition indexes (stable since CS:GO).
inline const std::unordered_map<std::string, uint16_t>& WeaponDefIndexes() {
    static const std::unordered_map<std::string, uint16_t> k = {
        {"weapon_deagle", 1},   {"weapon_elite", 2},      {"weapon_fiveseven", 3},
        {"weapon_glock", 4},    {"weapon_ak47", 7},       {"weapon_aug", 8},
        {"weapon_awp", 9},      {"weapon_famas", 10},     {"weapon_g3sg1", 11},
        {"weapon_galilar", 13}, {"weapon_m249", 14},      {"weapon_m4a4", 16},
        {"weapon_mac10", 17},   {"weapon_p90", 19},       {"weapon_mp5sd", 23},
        {"weapon_ump45", 24},   {"weapon_xm1014", 25},    {"weapon_bizon", 26},
        {"weapon_mag7", 27},    {"weapon_negev", 28},     {"weapon_sawedoff", 29},
        {"weapon_tec9", 30},    {"weapon_hkp2000", 32},   {"weapon_mp7", 33},
        {"weapon_mp9", 34},     {"weapon_nova", 35},      {"weapon_p250", 36},
        {"weapon_scar20", 38},  {"weapon_sg556", 39},     {"weapon_ssg08", 40},
        {"weapon_knife", 42},   {"weapon_knife_t", 59},   {"weapon_m4a1_silencer", 60},
        {"weapon_usp_silencer", 61}, {"weapon_cz75a", 63}, {"weapon_revolver", 64},
        // Knife models (used by the knife swapper).
        {"weapon_bayonet", 500},        {"weapon_knife_css", 503},
        {"weapon_knife_flip", 505},     {"weapon_knife_gut", 506},
        {"weapon_knife_karambit", 507}, {"weapon_knife_m9_bayonet", 508},
        {"weapon_knife_tactical", 509}, {"weapon_knife_falchion", 512},
        {"weapon_knife_survival_bowie", 514}, {"weapon_knife_butterfly", 515},
        {"weapon_knife_push", 516},     {"weapon_knife_cord", 517},
        {"weapon_knife_canis", 518},    {"weapon_knife_ursus", 519},
        {"weapon_knife_gypsy_jackknife", 520}, {"weapon_knife_outdoor", 521},
        {"weapon_knife_stiletto", 522}, {"weapon_knife_widowmaker", 523},
        {"weapon_knife_skeleton", 525}, {"weapon_knife_kukri", 526},
    };
    return k;
}

// Paint kit IDs for popular finishes (from items_game.txt).
inline const std::unordered_map<std::string, int>& PaintKits() {
    static const std::unordered_map<std::string, int> k = {
        // Finishes shared across many weapons
        {"fade", 38},           {"case_hardened", 44},  {"crimson_web", 12},
        {"slaughter", 59},      {"blue_steel", 42},     {"safari_mesh", 72},
        {"forest_ddpat", 5},    {"tiger_tooth", 409},   {"damascus_steel", 410},
        {"marble_fade", 413},   {"rust_coat", 414},     {"lore", 558},
        {"ultraviolet", 98},    {"night", 40},          {"fade_glock", 38},
        // Doppler family (knives / covert gems)
        {"doppler_p1", 415},    {"doppler_p2", 416},    {"doppler_p3", 417},
        {"doppler_p4", 418},    {"doppler_ruby", 419},  {"doppler_sapphire", 420},
        {"doppler_blackpearl", 421},
        {"gamma_doppler_p1", 569}, {"gamma_doppler_p2", 570},
        {"gamma_doppler_p3", 571}, {"gamma_doppler_p4", 572},
        {"gamma_doppler_emerald", 568},
        // Signature gun skins
        {"ak_redline", 282},        {"ak_asiimov", 801},    {"ak_vulcan", 302},
        {"ak_fire_serpent", 180},   {"ak_neon_revolution", 707},
        {"awp_asiimov", 279},       {"awp_dragon_lore", 344}, {"awp_medusa", 428},
        {"awp_lightning_strike", 51}, {"awp_redline", 259},
        {"m4a4_howl", 309},
        {"m4a1s_hyper_beast", 430}, {"usp_kill_confirmed", 504},
        {"deagle_blaze", 37},       {"deagle_code_red", 711},
        {"glock_water_elemental", 353},
    };
    return k;
}

// Resolves "ak47", "weapon_ak47", "7", "0x7" to a definition index.
inline bool ResolveWeapon(const std::string& name, uint16_t& out) {
    std::string key = name;
    for (auto& c : key)
        c = static_cast<char>(::tolower(c));
    if (key.rfind("weapon_", 0) != 0)
        key = "weapon_" + key;

    auto it = WeaponDefIndexes().find(key);
    if (it != WeaponDefIndexes().end()) {
        out = it->second;
        return true;
    }
    try {
        out = static_cast<uint16_t>(std::stoul(name, nullptr, 0));
        return true;
    } catch (...) {
        return false;
    }
}

inline bool ResolvePaintKit(const std::string& name, int& out) {
    std::string key = name;
    for (auto& c : key)
        c = static_cast<char>(::tolower(c));
    auto it = PaintKits().find(key);
    if (it != PaintKits().end()) {
        out = it->second;
        return true;
    }
    try {
        out = std::stoi(name, nullptr, 0);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace db
