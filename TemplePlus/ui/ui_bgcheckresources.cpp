#include "stdafx.h"
#include "ui_bgcheckresources.h"
#include "../bgcheckstate.h"
#include "party.h"
#include "inventory.h"
#include "critter.h"
#include "description.h"
#include "d20.h"
#include "d20_class.h"
#include "dispatcher.h"
#include "spell.h"
#include "obj.h"
#include "gamesystems/gamesystems.h"
#include "gamesystems/objects/objsystem.h"
#include "graphics/mapterrain.h"
#include "python/python_integration_spells.h"

namespace bgrolls {
namespace {
bool Effect(CheckResource &r,objHndl actor,SkillEnum skill) {
    // Guidance is spent on this skill check. Heroism remains as a real native
    // spell afterwards. Both use 3.5 typed bonuses, including suppression.
    if (r.spell.spellEnum==213) {r.value=1;r.type=34;return true;} // competence
    if (r.spell.spellEnum==557) {r.value=2;r.type=13;return true;}
    if (r.spell.spellEnum==558) {r.value=4;r.type=13;return true;} // morale
    int stat=-1;
    switch(r.spell.spellEnum) {
        case 44:stat=stat_strength;break;
        case 49:stat=stat_dexterity;break;
        case 548:stat=stat_charisma;break;
        case 549:stat=stat_intelligence;break;
        case 550:stat=stat_wisdom;break;
    }
    if (stat<0 || skillSys.skillPropsTable[skill].stat!=stat) return false;
    DispIoBonusList ability;
    dispatch.Dispatch10AbilityScoreLevelGet(actor,static_cast<Stat>(stat),&ability);
    int before=int(std::floor((ability.bonlist.GetEffectiveBonusSum()-10)/2.0));
    ability.bonlist.AddBonus(4,12,0); // enhancement; gloves/headband do not stack
    int after=int(std::floor((ability.bonlist.GetEffectiveBonusSum()-10)/2.0));
    r.value=after-before;r.type=0;
    return r.value>0;
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
bool ReliableCaster(objHndl owner,SpellStoreData spell) {
    if (d20Sys.d20Query(owner,DK_QUE_CannotCast)
        || d20Sys.d20Query(owner,DK_QUE_Critter_Is_Grappling)) return false;
    auto components=spell.GetSpellComponentFlags();
    if ((components&SpellComponent_Verbal) && (d20Sys.d20Query(owner,DK_QUE_Mute)
        || d20Sys.d20Query(owner,DK_QUE_Critter_Is_Deafened))) return false;
    if ((components&SpellComponent_Somatic) && spellSys.IsArcaneSpellClass(spell.classCode)) {
        auto classCode=spellSys.GetCastingClass(spell.classCode);
        for(int slot=int(EquipSlot::Helmet);slot<int(EquipSlot::Count);++slot)
            if (int(d20Sys.d20QueryWithData(owner,DK_QUE_Get_Arcane_Spell_Failure,classCode,slot))>0) return false;
    }
    // A cast needing an extra failure/concentration check is performed through
    // the normal spell action first, never granted for free by this preview.
    return true;
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
std::vector<CheckResource> ScanCheckResources(objHndl actor,SkillEnum skill) {
    std::vector<CheckResource> result;
    for (int n=0;n<party.GroupListGetLen();++n) {
        auto owner=party.GroupListGetMemberN(n);
        if (!owner || !objSystem->IsValidHandle(owner) || !party.IsInParty(owner)) continue;
        auto nearby=Near(actor,owner);
        auto obj=objSystem->GetObject(owner);
        for (bool spontaneous:{false,true}) {
            auto field=spontaneous ? obj_f_critter_spells_known_idx:obj_f_critter_spells_memorized_idx;
            auto spells=obj->GetSpellArray(field);
            for (size_t i=0;i<spells.GetSize();++i) {
                auto spell=obj->GetSpell(field,i);
                CheckResource r;r.owner=owner;r.spell=spell;r.slot=int(i);r.spontaneous=spontaneous;
                if (!Effect(r,actor,skill)) continue;
                auto natural=!spellSys.isDomainSpell(spell.classCode)
                    && d20ClassSys.IsNaturalCastingClass(spellSys.GetCastingClass(spell.classCode));
                if (natural!=spontaneous || d20Sys.d20Query(owner,DK_QUE_CannotCast)
                    || !spellSys.spellCanCast(owner,spell.spellEnum,spell.classCode,spell.spellLevel)) continue;
                if (!spontaneous && (spell.spellStoreState.usedUp
                    || spell.spellStoreState.spellStoreType!=SpellStoreType::spellStoreMemorized)) continue;
                auto remaining=spontaneous ? spellSys.GetNumSpellsPerDay(owner,spellSys.GetCastingClass(spell.classCode),spell.spellLevel)
                    -spellSys.NumSpellsInLevel(owner,obj_f_critter_spells_cast_idx,spell.classCode,spell.spellLevel):1;
                if (remaining<=0) continue;
                r.label=fmt::format("[L{} x{}] {}: {} (+{})",spell.spellLevel,remaining,
                    description.getDisplayName(owner),spellSys.GetSpellName(spell.spellEnum),r.value);
                auto reliable=ReliableCaster(owner,spell);r.available=nearby && reliable;
                if (!nearby) r.label="[far] "+r.label;
                else if (!reliable) r.label="[cast first] "+r.label;
                result.push_back(r);
            }
        }
        for (auto item:inventory.GetInventory(owner)) {
            auto itemObj=objSystem->GetObject(item);
            // Potions only. Scrolls and wands need their native activation checks
            // and are not silently treated as guaranteed consumables.
            if (itemObj->type!=obj_t_food || itemObj->GetSpellArray(obj_f_item_spell_idx).GetSize()!=1) continue;
            CheckResource r;r.owner=owner;r.item=item;r.available=nearby;r.spell=itemObj->GetSpell(obj_f_item_spell_idx,0);
            r.quantity=inventory.GetQuantity(item);r.charges=itemObj->GetInt32(obj_f_item_spell_charges_idx);
            if (!Effect(r,actor,skill) || r.quantity<1 || r.charges<1) continue;
            r.label=fmt::format("{}: {} x{} (+{})",description.getDisplayName(owner),description.getDisplayName(item),r.quantity,r.value);
            if (!nearby) r.label="[far] "+r.label;
            result.push_back(r);
        }
    }
    return result;
}
bool ValidateCheckResources(objHndl actor,const std::vector<CheckResource>& resources) {
    std::vector<std::pair<uint64_t,int>> paid;
    bgchecks::SlotBudget budget;
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
                || !ReliableCaster(r.owner,spell)
                || d20Sys.d20Query(r.owner,DK_QUE_CannotCast)
                || !spellSys.spellCanCast(r.owner,spell.spellEnum,spell.classCode,spell.spellLevel)) return false;
            if (r.spontaneous) {
                auto remaining=spellSys.GetNumSpellsPerDay(r.owner,spellSys.GetCastingClass(spell.classCode),spell.spellLevel)
                    -spellSys.NumSpellsInLevel(r.owner,obj_f_critter_spells_cast_idx,spell.classCode,spell.spellLevel);
                if (!budget.Reserve(r.owner.handle,spell.classCode,spell.spellLevel,remaining)) return false;
            }
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
        if (r.spell.spellEnum!=213) {
            auto packet=ResourcePacket(r);
            packet.targetCount=packet.orgTargetCount=1;packet.targetListHandles[0]=actor;
            auto id=spellSys.GetNewSpellId();
            if (!spellSys.RegisterSpell(packet,id)) throw std::runtime_error("cannot register Heroism bonus spell");
            pySpellIntegration.SpellTrigger(id,SpellEvent::SpellEffect);
        }
    }
}
}
