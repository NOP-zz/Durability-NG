#undef GetObject

#include "Events.h"
#include "Settings.h"
#include "Tools.h"

namespace DurabilityNG {

inline bool CanBlock(RE::TESForm *form) {
    return form && (
        form->IsWeapon() || 
        (form->IsArmor() && form->As<RE::TESObjectARMO>()->IsShield())
    );
};

template <class T>
class PickOne {
	public:
		inline bool Has() { return _weight > 0.0; };
		inline T Get(T fallback) { return Has() ? _curr : fallback; };
		inline void Push(T cand, float weight = 1.0) {
			_weight += weight;
			if (Tools::RandU<float>(_weight) < weight) _curr = cand;
		};
	private:
		T _curr;
		double _weight = 0;
};

template <class T, typename W = float>
class PickList {
	public:
		inline double weightSum() { return _weightSum; };
		inline void Push(const T&& val, const W& w) {
			if (w > 0.0) {
				_entries.emplace_back(val, w);
				_weightSum += w;
			}
		};

		inline T* Pull(W* weight1 = nullptr) {
			if (0.0 < _weightSum) {
				auto v = Tools::RandU(_weightSum);

				for (auto& [item, w]: _entries) {
					if (v < w) {
						_weightSum -= w;
						if (weight1) *weight1 = w;
						w = 0;
						return &item;
					}
					v -= w;
				}
			}
			return nullptr;
		};

		PickList(unsigned int reserve = 0) {
			_entries.reserve(reserve);
		};

	private:
		std::vector<std::pair<T, W>> _entries = {};
		double _weightSum = 0;
};

class HitEventHandler : public RE::BSTEventSink<RE::TESHitEvent> {
public:
	static HitEventHandler* GetSingleton() {
        static HitEventHandler singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* event, RE::BSTEventSource<RE::TESHitEvent>* eventSource) override {
		if (!event) return RE::BSEventNotifyControl::kContinue;

		auto* settings = DurabilityNG::Settings::GetSingleton();
		bool is_magic = false;

		const auto& attacker = event->cause->As<RE::Actor>();
		const auto& defender = event->target->As<RE::Actor>();

		do {
			if (event->projectile) break;
			if (!event->source) break;
			const auto& info = settings->Attack.ActorInfo(attacker, event->flags);
			if (!info) break;
			const auto& proc = attacker->GetActorRuntimeData().currentProcess;
			if (!proc) break;
			if (!proc->high) break;
			if (!proc->high->attackData) break;
			if (proc->high->attackData->data.attackSpell) is_magic = true;
			if (!proc->middleHigh) break;
			bool left = proc->high->attackData->IsLeftAttack(); // definition may be wrong
			attacker->GetGraphVariableBool("bLeftHandAttack", left);
			const auto& entry = left ? proc->middleHigh->leftHand : proc->middleHigh->rightHand;
			settings->Degrade(info, attacker, entry, event->flags, left);
		} while(false);

		if (const auto& info = settings->Defense.ActorInfo(attacker, event->flags))
			if (const auto& proc = defender->GetActorRuntimeData().currentProcess) {
				PickOne<RE::TESForm *> pick;
				bool blocked = event->flags.any(RE::TESHitEvent::Flag::kHitBlocked);
				uint32_t armorRaw = 0;
				for (const auto& eqObj : proc->equippedForms) {
					if (!eqObj.object->GetPlayable()) continue;
					float weight;
					if (const auto *armor = eqObj.object->As<RE::TESObjectARMO>()) {
						if (!armor->armorRating && settings->ignoreZeroArmor) continue;
						armorRaw += armor->armorRating;
						weight = armor->weight + armor->armorRating * 0.01;
					} else if (const auto *weapon = eqObj.object->As<RE::TESObjectWEAP>())
        				weight = weapon->weight * (1 + weapon->GetStagger());
					else
						continue;
					if (blocked && !CanBlock(eqObj.object))
						weight *= settings->blockedHitOther;
					if (weight > 0.0)
						pick.Push(eqObj.object, weight);
				}

				if (pick.Has())
					if (auto inv = defender->GetInventoryChanges(); inv && inv->entryList)
						for (auto& entry : *inv->entryList)
							if (entry && entry->object == pick.Get(NULL)) {
								bool left = blocked && CanBlock(entry->object);
								float mult = armorRaw * 0.01 / defender->AsActorValueOwner()->GetActorValue(RE::ActorValue::kDamageResist);
								settings->Degrade(info, defender, entry, event->flags, left, mult);
								break;
							}
			}
		
		do {
			const auto& info = settings->Destroy.ActorInfo(attacker, event->flags);
			if (!info) break;
			if (!(info >= 1.0 || info > Tools::RandU<float>())) break;
			auto invCh = defender->GetInventoryChanges();
			if (!invCh) break;
			auto weight = invCh->totalWeight - invCh->armorWeight;
			if (!(weight > 0.0)) break;
			if (const auto& proc = attacker->GetActorRuntimeData().currentProcess; proc && proc->middleHigh) {
				if (proc->middleHigh->leftHand) weight -= proc->middleHigh->leftHand->GetWeight();
				if (proc->middleHigh->rightHand) weight -= proc->middleHigh->rightHand->GetWeight();
			}
			if (!(weight > 0.0)) break;
			auto resist = settings->DestroyResist(attacker);
			if (resist > weight) break;
			if (resist > Tools::RandU(weight)) break;
		
			std::map<RE::TESBoundObject*, std::int32_t> counts{};
			if (auto cont = defender->GetContainer())
				for (auto && ent : std::span(cont->containerObjects, cont->numContainerObjects)) {
					const auto& [it, added] = counts.try_emplace(ent->obj, ent->count);
					if (!added) it->second += ent->count;
				}
			
			struct Item { RE::TESBoundObject* form; std::int32_t count; };
			auto pick = PickList<Item>(defender->IsPlayer() ? 100 : 20);
			if (invCh->entryList)
				for (auto &entry : *invCh->entryList) {
					if (entry->IsQuestObject()) continue;
					if (entry->IsLeveled()) continue;
					const auto& obj = entry->object;
					auto num = entry->countDelta;
					if (auto it = counts.find(obj); it != counts.end())
						num += it->second;
					if (entry->extraLists && (obj->IsArmor() || obj->IsWeapon()))
						for (auto &edl : *entry->extraLists) {
							if (edl->HasType<RE::ExtraWorn>()) num--;
							if (edl->HasType<RE::ExtraWornLeft>()) num--;							
						}
					if (num > 0) {
						auto w = settings->DestroyWeight(obj) * num;
						if (entry->IsFavorited()) w *= settings->destroyFavorite;
						pick.Push({obj, num}, w);
					}
				}

				
			bool message = settings->destroyMessage && defender->IsPlayer();
			int32_t more = 0;
			std::string msg = "";
			if (message)
				msg.reserve(30 + std::min(100u, settings->destroyMessage));
			while (auto item = pick.Pull()) {
				weight -= item->form->GetWeight() * item->count;
				if((item->count = Tools::RandU(item->count))) { // TODO? add exponent (default 2)
					if (message) {
						if (msg.size() > settings->destroyMessage)
							more += item->count;
						else {
							auto name = item->form->GetName();
							if (name && name[0]) {
								if (!msg.empty()) msg += ", ";
								if (item->count > 1) msg += std::to_string(item->count) + " ";
								msg += name;
							} else
								more += item->count;
						}
					}
					defender->RemoveItem(item->form, item->count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);				
				}
				if (resist > Tools::RandU(weight)) break;
			}
			if (more) {
				if (!msg.empty()) msg += ", and ";
				msg += std::to_string(more) + " items";
			}
			if (message && msg.size()) {
				msg.insert(0, "Destroyed ");
				RE::DebugNotification(msg.c_str());
			}

		} while (false);
		
        return RE::BSEventNotifyControl::kContinue;
    }

    static void Register() {
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(GetSingleton());
    }

};

void InitEvents() {
	HitEventHandler::Register();
}
}
