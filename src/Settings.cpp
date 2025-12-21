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
    const GroupActorInfo& info,
    RE::Actor *subject,
    RE::InventoryEntryData *entry,
    const HitFlags& flag,
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
        float destroy = Break.ActorInfo(subject);
        if (!(destroy > 1.0)) break;
        if (entry->IsQuestObject()) break;
        if (exp != 1.0) destroy *= pow(cur, exp);
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

GroupActorInfo Group::ActorInfo(const RE::Actor *target)
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
    return std::isfinite(ret) && ret > 0.0 ? ret : 0.0;
}

}