#pragma once
#include "common.h"
#include "spell_structs.h"
#include "skill.h"

namespace bgrolls {
// Explicit adapters, rather than guesses from an item's name. Active equipment,
// feats and effects are already in the native BonusList and are never charged.
struct CheckResource {
    objHndl owner, item;
    SpellStoreData spell;
    int slot=-1, quantity=0, charges=0, value=0, type=0;
    std::string label;
    bool selected=false, spontaneous=false;
};
std::vector<CheckResource> ScanCheckResources(objHndl actor,SkillEnum skill);
bool ValidateCheckResources(objHndl actor,const std::vector<CheckResource>& resources);
void CommitCheckResources(objHndl actor,const std::vector<CheckResource>& resources);
}
