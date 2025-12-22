#include "Settings.h"

namespace DurabilityNG {


inline RE::ExtraDataList* GetWorn(RE::BSSimpleList<RE::ExtraDataList*>* extraLists, bool left = false) {
    if (extraLists) {
        auto edt = left ? RE::ExtraDataType::kWornLeft : RE::ExtraDataType::kWorn;
        for (auto& edl : *extraLists)
            if (edl->HasType(edt))
                return edl;
    }
    return nullptr;
};

void Settings::Degrade(
    const GroupActorInfo &info,
    RE::Actor *subject,
    RE::InventoryEntryData *entry,
    const HitFlags &flags,
    bool left,
    float mult
) {
    if (!subject) return;
    if (!entry) return;
    if (!info) return;
    auto *form = entry->object;
    if (!form) return;
    if (!form->GetPlayable()) return;
    auto *worn = GetWorn(entry->extraLists, left);
    if (!worn) return;

    mult *= info;
    mult *= GetMult(entry->object->As<RE::BGSKeywordForm>());
    if (!(mult > 0.0)) return;
    
    auto *edHealth = worn->GetByType<RE::ExtraHealth>();
    const float old = edHealth ? edHealth->health : 1.0;
    const float cur = std::max(minHealth, old - mult);
    
    do {
        float exp = breakExponent[cur >= 1.0];
        if (!std::isfinite(exp)) break;
        float destroy = Break.ActorInfo(subject, flags);
        if (!(destroy > 1.0)) break;
        if (entry->IsQuestObject()) break;
        if (exp) destroy *= pow(cur, exp);
        if (destroy > Tools::RandU<float>()) break;

        if (breakMessage && subject->IsPlayer()) {
            std::string msg = "Destroyed worn ";
            msg += worn->GetDisplayName(form);
            RE::DebugNotification(msg.c_str());
        }

        subject->RemoveItem(form, 1, RE::ITEM_REMOVE_REASON::kRemove, worn, NULL);
        return;
    } while(false);
    
    if (cur < old) {
        if (edHealth) edHealth->health = cur;
        else worn->Add(new RE::ExtraHealth(cur));

        subject->AddChange(0x8000420); // 5 10 27
        subject->OnArmorActorValueChanged();
    }
}

float Settings::DestroyWeight(RE::TESBoundObject *form)
{
    auto res = form->GetWeight();
    if (destroyMaterialExponent > 0.0)
        res *= pow(GetMult(form->As<RE::BGSKeywordForm>()), destroyMaterialExponent);
    return res;
}

float Settings::DestroyResist(RE::Actor *subject)
{
    float res = destroyResistBase;
    float dr = subject->AsActorValueOwner()->GetActorValue(RE::ActorValue::kDamageResist);
    if (dr > 0.0 && destroyResistPivot > 0.0 && destroyResistExponent > 0.0)
        res += destroyResistPivot * pow(dr / destroyResistPivot, destroyResistExponent);
    return res > 1.0 ? res : 1.0;
}

Settings *Settings::GetSingleton()
{
    static Settings singleton;
	return std::addressof(singleton);
}

float Settings::GetMult(const RE::BGSKeywordForm *form)
{
    if (!form) return 1.0;
    float oth = 1.0;
    float mat = 1.0;
    int materials = 0;
    for (const auto& kw : form->GetKeywords()) {
        const auto val = kw2mul.find(kw->GetFormID());
        if (val != kw2mul.cend()) {
            if (val->second == 0.0) return 0.0;
            if (val->second > 0.0) {
                materials++;
                mat *= val->second;
            } else
                oth *= -val->second;
        }
    }
    switch (materials) {
        case 0: mat = noMaterialMult; break;
        case 1: break;
        case 2: mat = std::sqrt(mat); break;
        case 3: mat = std::cbrt(mat); break;
        default: mat = std::pow(mat, 1.0/materials); break;
    }
    return mat * oth;
}

GroupActorInfo Group::ActorInfo(const RE::Actor *target, const HitFlags& flags)
{
    if (!target) return 0.0;
    float ret = Global;
    if (target->IsPlayer()) ret *= Player;
    else {
        ret *= NPC;
        if (target->IsPlayerTeammate()) ret *= Teammate;
        if (target->IsEssential()) ret *= Essential;
        else if (target->IsProtected()) ret *= Protected;
        const auto& base = target->GetActorBase();
        if (base && base->IsUnique()) ret *= Unique;
        ret *= Respawns[base && base->Respawns()];
    }
    if (flags.any(RE::TESHitEvent::Flag::kPowerAttack)) ret *= Power;
    if (flags.any(RE::TESHitEvent::Flag::kSneakAttack)) ret *= Sneak;
    if (flags.any(RE::TESHitEvent::Flag::kBashAttack )) ret *= Bash;
    if (flags.any(RE::TESHitEvent::Flag::kHitBlocked )) ret *= Block;
    return std::isfinite(ret) && ret > 0.0 ? ret : 0.0;
}

void Group::Load(CSimpleIniA& ini, const char * section)
{
    if (auto v = ini.GetDoubleValue(section, "Global"     , -fInf); v >= 0.0) Global      = v;
    if (auto v = ini.GetDoubleValue(section, "Player"     , -fInf); v >= 0.0) Player      = v;
    if (auto v = ini.GetDoubleValue(section, "NPC"        , -fInf); v >= 0.0) NPC         = v;
    if (auto v = ini.GetDoubleValue(section, "Teammate"   , -fInf); v >= 0.0) Teammate    = v;
    if (auto v = ini.GetDoubleValue(section, "Essential"  , -fInf); v >= 0.0) Essential   = v;
    if (auto v = ini.GetDoubleValue(section, "Protected"  , -fInf); v >= 0.0) Protected   = v;
    if (auto v = ini.GetDoubleValue(section, "Unique"     , -fInf); v >= 0.0) Unique      = v;
    if (auto v = ini.GetDoubleValue(section, "RespawnsNot", -fInf); v >= 0.0) Respawns[0] = v;
    if (auto v = ini.GetDoubleValue(section, "Respawns"   , -fInf); v >= 0.0) Respawns[1] = v;
    if (auto v = ini.GetDoubleValue(section, "Power"      , -fInf); v >= 0.0) Power       = v;
    if (auto v = ini.GetDoubleValue(section, "Sneak"      , -fInf); v >= 0.0) Sneak       = v;
    if (auto v = ini.GetDoubleValue(section, "Bash"       , -fInf); v >= 0.0) Bash        = v;
    if (auto v = ini.GetDoubleValue(section, "Block"      , -fInf); v >= 0.0) Block       = v;
}

void Settings::Load(CSimpleIniA &ini)
{
    Attack .Load(ini, "Attack");
    Defense.Load(ini, "Defense");
    Break  .Load(ini, "Break");
    Destroy.Load(ini, "Destroy");

    ignoreZeroArmor = ini.GetBoolValue("Break", "ignoreZeroArmor", ignoreZeroArmor);
    breakMessage    = ini.GetBoolValue("Break", "Message"        , breakMessage);
    
    if (auto v = ini.GetDoubleValue("Defense", "blockedHitOther" , -fInf); std::isfinite(v) && v >= 0.0) blockedHitOther         = v;
    if (auto v = ini.GetDoubleValue("Break"  , "ExponentLow"     , -fInf); !std::isinf(v)              ) breakExponent[0]        = v;
    if (auto v = ini.GetDoubleValue("Break"  , "ExponentHigh"    , -fInf); !std::isinf(v)              ) breakExponent[1]        = v;
    if (auto v = ini.GetDoubleValue("Destroy", "Favorite"        , -fInf); std::isfinite(v) && v >= 0.0) destroyFavorite         = v;
    if (auto v = ini.GetDoubleValue("Destroy", "ResistBase"      , -fInf); std::isfinite(v)            ) destroyResistBase       = v;
    if (auto v = ini.GetDoubleValue("Destroy", "ResistPivot"     , -fInf); std::isfinite(v) && v >= 0.0) destroyResistPivot      = v;
    if (auto v = ini.GetDoubleValue("Destroy", "ResistExponent"  , -fInf); std::isfinite(v)            ) destroyResistExponent   = v;
    if (auto v = ini.GetDoubleValue("Destroy", "MaterialExponent", -fInf); std::isfinite(v)            ) destroyMaterialExponent = v;
    
    if (auto v = ini.GetLongValue("Destroy", "Message", -1); v >= 0) destroyMessage = v;

    CSimpleIniA::TNamesDepend list;
    ini.GetAllKeys("Materials", list);
    for (auto& mat : list)
        if (auto v = ini.GetDoubleValue("Materials", mat.pItem, -1.0); v >= 0.0)
            if (strcmp(mat.pItem, "__default__"))
                if (auto kw = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(mat.pItem))
                    kw2mul.try_emplace(kw->GetFormID(), v);
                else
                    SKSE::log::warn(std::format("unknown keyword: {}", mat.pItem));
            else
                noMaterialMult = v;
        else
            SKSE::log::warn(std::format("invalid value for keyword: {}", mat.pItem));

    SKSE::log::info("settings loaded");
}


void InitSettings(const char* path) {
    CSimpleIniA ini;
    ini.SetUnicode();
    if (SI_OK == ini.LoadFile(path))
        Settings::GetSingleton()->Load(ini);
    else
        SKSE::log::warn("loading settings failed");
}

}