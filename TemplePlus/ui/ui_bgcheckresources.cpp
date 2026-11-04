#include "stdafx.h"
#include "ui_bgcheckresources.h"
#include "party.h"
#include "inventory.h"
#include "critter.h"
#include "description.h"
#include "d20.h"
#include "spell.h"
#include "obj.h"
#include "gamesystems/gamesystems.h"
#include "gamesystems/objects/objsystem.h"
#include "graphics/mapterrain.h"
#include "python/python_integration_spells.h"

namespace bgrolls {
namespace {
bool Effect(CheckResource &r) {
    // Guidance is spent on this skill check. Heroism remains as a real native
    // spell afterwards. Both use 3.5 typed bonuses, including suppression.
    if (r.spell.spellEnum==213) {r.value=1;r.type=34;return true;} // competence
    if (r.spell.spellEnum==557) {r.value=2;r.type=13;return true;} // morale
    return false;
}
bool Near(objHndl actor,objHndl owner) {
    if (!objSystem->IsValidHandle(owner) || !party.IsInParty(owner) || critterSys.IsDeadOrUnconscious(owner)) return false;
    auto &terrain=gameSystems->GetTerrain();
    if (terrain.SceneAreaForActor(actor.handle)!=terrain.SceneAreaForActor(owner.handle)) return false;
    auto a=objects.GetLocationFull(actor).ToInches2D(),b=objects.GetLocationFull(owner).ToInches2D();
    return std::hypot(a.x-b.x,a.y-b.y)<=60; // native touch range, no remote assistance
}
bool SameSpell(const SpellStoreData &a,const SpellStoreData &b) {
    return a.spellEnum==b.spellEnum && a.classCode==b.classCode && a.spellLevel==b.spellLevel
        && a.metaMagicData==b.metaMagicData;
}
}
std::vector<CheckResource> ScanCheckResources(objHndl actor) {
    std::vector<CheckResource> result;
    for (int n=0;n<party.GroupListGetLen();++n) {
        auto owner=party.GroupListGetMemberN(n);
        if (!Near(actor,owner)) continue;
        auto obj=objSystem->GetObject(owner);
        auto spells=obj->GetSpellArray(obj_f_critter_spells_memorized_idx);
        for (size_t i=0;i<spells.GetSize();++i) {
            auto spell=obj->GetSpell(obj_f_critter_spells_memorized_idx,i);
            CheckResource r;r.owner=owner;r.spell=spell;r.slot=int(i);
            if (!Effect(r) || spell.spellStoreState.usedUp
                || spell.spellStoreState.spellStoreType!=SpellStoreType::spellStoreMemorized
                || d20Sys.d20Query(owner,DK_QUE_CannotCast)
                || !spellSys.spellCanCast(owner,spell.spellEnum,spell.classCode,spell.spellLevel)) continue;
            r.label=fmt::format("{}: {} (+{})",description.getDisplayName(owner),spellSys.GetSpellName(spell.spellEnum),r.value);
            result.push_back(r);
        }
        for (auto item:inventory.GetInventory(owner)) {
            auto itemObj=objSystem->GetObject(item);
            // Potions only. Scrolls and wands need their native activation checks
            // and are not silently treated as guaranteed consumables.
            if (itemObj->type!=obj_t_food || itemObj->GetSpellArray(obj_f_item_spell_idx).GetSize()!=1) continue;
            CheckResource r;r.owner=owner;r.item=item;r.spell=itemObj->GetSpell(obj_f_item_spell_idx,0);
            r.quantity=inventory.GetQuantity(item);r.charges=itemObj->GetInt32(obj_f_item_spell_charges_idx);
            if (!Effect(r) || r.quantity<1 || r.charges<1) continue;
            r.label=fmt::format("{}: {} x{} (+{})",description.getDisplayName(owner),description.getDisplayName(item),r.quantity,r.value);
            result.push_back(r);
        }
    }
    return result;
}
bool ValidateCheckResources(objHndl actor,const std::vector<CheckResource>& resources) {
    std::vector<std::pair<uint64_t,int>> paid;
    for (const auto &r:resources) if (r.selected) {
        if (!Near(actor,r.owner)) return false;
        auto identity=std::make_pair(r.item ? r.item.handle:r.owner.handle,r.item ? -1:r.slot);
        if (std::find(paid.begin(),paid.end(),identity)!=paid.end()) return false;
        paid.push_back(identity);
        if (r.item) {
            if (!objSystem->IsValidHandle(r.item) || inventory.GetParent(r.item)!=r.owner) return false;
            auto item=objSystem->GetObject(r.item);
            if (item->type!=obj_t_food || inventory.GetQuantity(r.item)!=r.quantity
                || item->GetInt32(obj_f_item_spell_charges_idx)!=r.charges
                || item->GetSpellArray(obj_f_item_spell_idx).GetSize()!=1
                || !SameSpell(item->GetSpell(obj_f_item_spell_idx,0),r.spell)) return false;
        } else {
            auto owner=objSystem->GetObject(r.owner);
            if (r.slot<0 || size_t(r.slot)>=owner->GetSpellArray(obj_f_critter_spells_memorized_idx).GetSize()) return false;
            auto spell=owner->GetSpell(obj_f_critter_spells_memorized_idx,r.slot);
            if (!SameSpell(spell,r.spell) || spell.spellStoreState.usedUp
                || d20Sys.d20Query(r.owner,DK_QUE_CannotCast)
                || !spellSys.spellCanCast(r.owner,spell.spellEnum,spell.classCode,spell.spellLevel)) return false;
        }
    }
    return true;
}
void CommitCheckResources(objHndl actor,const std::vector<CheckResource>& resources) {
    if (!ValidateCheckResources(actor,resources)) throw std::invalid_argument("skill bonus resource changed; action cancelled");
    for (const auto &r:resources) if (r.selected) {
        // All preconditions were validated before the first charge. The modal
        // owns a time lock, so no heartbeat can spend a later source meanwhile.
        if (r.item) inventory.ItemSpellChargeConsume(r.item);
        else {
            D20SpellData data;data.Set(r.spell.spellEnum,r.spell.classCode,r.spell.spellLevel,INV_IDX_INVALID,r.spell.metaMagicData);
            SpellPacketBody packet(r.owner,data);packet.Debit();
        }
        if (r.spell.spellEnum==557) {
            D20SpellData data;data.Set(557,r.spell.classCode,r.spell.spellLevel,INV_IDX_INVALID,r.spell.metaMagicData);
            SpellPacketBody packet(r.owner,data);
            if (r.item) packet.casterLevel=(std::max)(1u,2*r.spell.spellLevel-1);
            packet.targetCount=packet.orgTargetCount=1;packet.targetListHandles[0]=actor;
            auto id=spellSys.GetNewSpellId();
            if (!spellSys.RegisterSpell(packet,id)) throw std::runtime_error("cannot register Heroism bonus spell");
            pySpellIntegration.SpellTrigger(id,SpellEvent::SpellEffect);
        }
    }
}
}
