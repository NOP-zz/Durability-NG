#pragma once

#include <limits>
#include "Tools.h"
#include "SimpleIni.h"

namespace DurabilityNG {

using GroupActorInfo = float;
using HitFlags = REX::EnumSet<RE::TESHitEvent::Flag, std::uint8_t>;
constexpr float fNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float fInf = std::numeric_limits<float>::infinity();
    
class Group {
    public:
        GroupActorInfo ActorInfo(const RE::Actor* target, const HitFlags& flags);
        void Load(CSimpleIniA& ini, const char * section);
    private:
        float Global = 0.0;
        float Player = 1.0, NPC = 1.0;
        float Teammate = 0.0;
        float Essential = 1.0, Protected = 1.0;
        float Unique = 0.0;
        float Respawns[2] = {0.0, 1.0};

        float Power = 3.0;
        float Sneak = 0.5;
        float Bash = 1.5;
        float Block = 0.7;
};

class Settings {

    public:
        Group Attack, Defense, Break, Destroy;
        
        float minHealth = 0.01;
        float blockedHitOther = 0.3; // odds: blocked, still hit non-block implement
        
        bool ignoreZeroArmor = true;
        float breakExponent[2] = {-0.5, fNaN};
        bool breakMessage = true;
        
        // Destroy
        float destroyFavorite = 0.50;
        float destroyResistBase = 50.0;
        float destroyResistPivot = 100.0;
        float destroyResistExponent = 0.5;
        float destroyMaterialExponent = 0.5;
        uint32_t destroyMessage = 100;
        
        void Load(CSimpleIniA& ini);
        
        void Degrade(
            const GroupActorInfo& info,
            RE::Actor *subject,
            RE::InventoryEntryData *entry,
            const HitFlags& flags,
            bool left,
            float mult = 1.0
        );

        float DestroyWeight(RE::TESBoundObject* form);
        float DestroyResist(RE::Actor *subject);

        static Settings* GetSingleton();
    private:
        

        float GetMult(const RE::BGSKeywordForm *form);
        std::unordered_map<RE::FormID, float> kw2mul;
        float noMaterialMult = 2.5;
        
};

void InitSettings(const char* path);

}