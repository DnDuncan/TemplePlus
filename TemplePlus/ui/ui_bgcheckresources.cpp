#include "stdafx.h"
#include "ui_bgcheckresources.h"
#include "party.h"
#include "inventory.h"
#include "critter.h"
#include "description.h"
#include "d20.h"
#include "d20_class.h"
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
    if (r.spell.spellEnum==557) {r.value=2;r.type=13;return true;}
    if (r.spell.spellEnum==558) {r.value=4;r.type=13;return true;} // morale
    return false;
}
bool Near(objHndl actor,objHndl owner) {
    if (!objSystem->IsValidHandle(owner) || !party.IsInParty(owner) || critterSys.IsDeadOrUnconscious(owner)) return false;
    auto &terrain=gameSystems->GetTerrain();
    if (terrain.SceneAreaForActor(actor.handle)!=terrain.SceneAreaForActor(owner.handle)) return false;
    auto a=objects.GetLocationFull(actor).ToInches2D(),b=objects.GetLocationFull(owner).ToInches2D();
    return std::hypot(a.x-b.x,a.y-b.y)<=60 && (actor==owner || critterSys.HasLineOfSight(owner,actor)==0); // native touch range, no remote assistance
}
bool SameSpell(const SpellStoreData &a,const SpellStoreData &b) {
    auto am=a.metaMagicData,bm=b.metaMagicData;
    return a.spellEnum==b.spellEnum && a.classCode==b.classCode && a.spellLevel==b.spellLevel
        && uint32_t(am)==uint32_t(bm);
}
SpellPacketBody ResourcePacket(const CheckResource &r) {
    // Initialize explicitly: the legacy (caster,data) convenience constructor
    // shadows spellEnum and can leave it zero. Never register a zero spell.
    SpellPacketBody packet;
    packet.spellEnum=packet.spellEnumOriginal=r.spell.spellEnum;packet.caster=r.owner;
    packet.spellClass=r.spell.classCode;packet.spellKnownSlotLevel=r.spell.spellLevel;
    auto mm=r.spell.metaMagicData;packet.metaMagicData=uint32_t(mm);packet.invIdx=INV_IDX_INVALID;
    if (r.item) packet.casterLevel=(std::max)(1,2*int(r.spell.spellLevel)-1);
    else spellSys.SpellPacketSetCasterLevel(&packet);
    return packet;
}

}
std::vector<CheckResource> ScanCheckResources(objHndl actor) {
    std::vector<CheckResource> result;
    for (int n=0;n<party.GroupListGetLen();++n) {
        auto owner=party.GroupListGetMemberN(n);
        if (!Near(actor,owner)) continue;
        auto obj=objSystem->GetObject(owner);
        for (bool spontaneous:{false,true}) {
            auto field=spontaneous ? obj_f_critter_spells_known_idx:obj_f_critter_spells_memorized_idx;
            auto spells=obj->GetSpellArray(field);
            for (size_t i=0;i<spells.GetSize();++i) {
                auto spell=obj->GetSpell(field,i);
                CheckResource r;r.owner=owner;r.spell=spell;r.slot=int(i);r.spontaneous=spontaneous;
                if (!Effect(r)) continue;
                auto natural=!spellSys.isDomainSpell(spell.classCode)
                    && d20ClassSys.IsNaturalCastingClass(spellSys.GetCastingClass(spell.classCode));
                if (natural!=spontaneous || d20Sys.d20Query(owner,DK_QUE_CannotCast)
                    || d20Sys.d20Query(owner,DK_QUE_Mute)
                    || !spellSys.spellCanCast(owner,spell.spellEnum,spell.classCode,spell.spellLevel)) continue;
                if (!spontaneous && (spell.spellStoreState.usedUp
                    || spell.spellStoreState.spellStoreType!=SpellStoreType::spellStoreMemorized)) continue;
                if (spontaneous && spellSys.NumSpellsInLevel(owner,obj_f_critter_spells_cast_idx,spell.classCode,spell.spellLevel)
                    >=spellSys.GetNumSpellsPerDay(owner,spellSys.GetCastingClass(spell.classCode),spell.spellLevel)) continue;
                r.label=fmt::format("{}: {} (+{})",description.getDisplayName(owner),spellSys.GetSpellName(spell.spellEnum),r.value);
                result.push_back(r);
            }
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
        auto identity=std::make_pair(r.item ? r.item.handle:r.owner.handle,r.item ? -1:r.spontaneous ? -2-r.slot:r.slot);
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
            auto field=r.spontaneous ? obj_f_critter_spells_known_idx:obj_f_critter_spells_memorized_idx;
            if (r.slot<0 || size_t(r.slot)>=owner->GetSpellArray(field).GetSize()) return false;
            auto spell=owner->GetSpell(field,r.slot);
            if (!SameSpell(spell,r.spell) || (!r.spontaneous && spell.spellStoreState.usedUp)
                || d20Sys.d20Query(r.owner,DK_QUE_Mute)
                || d20Sys.d20Query(r.owner,DK_QUE_CannotCast)
                || !spellSys.spellCanCast(r.owner,spell.spellEnum,spell.classCode,spell.spellLevel)) return false;
            if (r.spontaneous && spellSys.NumSpellsInLevel(r.owner,obj_f_critter_spells_cast_idx,spell.classCode,spell.spellLevel)
                >=spellSys.GetNumSpellsPerDay(r.owner,spellSys.GetCastingClass(spell.classCode),spell.spellLevel)) return false;
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
            auto packet=ResourcePacket(r);packet.Debit();
        }
        if (r.spell.spellEnum==557 || r.spell.spellEnum==558) {
            auto packet=ResourcePacket(r);
            packet.targetCount=packet.orgTargetCount=1;packet.targetListHandles[0]=actor;
            auto id=spellSys.GetNewSpellId();
            if (!spellSys.RegisterSpell(packet,id)) throw std::runtime_error("cannot register Heroism bonus spell");
            pySpellIntegration.SpellTrigger(id,SpellEvent::SpellEffect);
        }
    }
}
}
