#pragma once

#include <limits>
#include "Tools.h"

namespace DurabilityNG {

using GroupActorInfo = float;
constexpr float fNaN = std::numeric_limits<float>::quiet_NaN();
    
class Group {
    public:
        GroupActorInfo ActorInfo(const RE::Actor* target);
    private:
        float Global;
        float Player, NPC;
        float Teammate;
        float Essential, Protected;
        float Unique;
        float Respawns[2];
};

class Settings {
    using HitFlags = REX::EnumSet<RE::TESHitEvent::Flag, std::uint8_t>;

    public:
        Group Attack, Defense, Break, Destroy;
        
        float minHealth = 0.01;
        float blockedHitOther = 0.0; // odds: blocked, still hit non-block implement
        
        bool ignoreZeroArmor = true;
        float breakExponent[2] = {0.1, fNaN};
        bool breakMessage = true;
        
        float destroyFavorite = 0.50;
        float destroyResistBase = 50.0;
        float destroyResistPivot = 100.0;
        float destroyResistExponent = 0.5;
        float destroyMaterialExponent = 0.5;
        uint32_t destroyMessage = 100;
        
        
        
        void Degrade(
            const GroupActorInfo& info,
            RE::Actor *subject,
            RE::InventoryEntryData *entry,
            const HitFlags& flag,
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

}