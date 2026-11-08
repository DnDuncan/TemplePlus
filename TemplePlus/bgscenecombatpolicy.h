#pragma once
namespace bgscenecombatpolicy {
inline bool Join(int combatScene,int actorScene,bool civilian) {return !civilian && (!combatScene || combatScene==actorScene);}
inline bool Target(int actorScene,int targetScene,bool civilian) {return !civilian && actorScene && actorScene==targetScene;}
inline bool Free(bool inInitiative,bool party,bool civilian,int actorScene,int combatScene) {return !inInitiative && (party || civilian || actorScene!=combatScene);}
}
