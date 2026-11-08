#pragma once
#include <cstdint>
#include <map>
#include <utility>

namespace bgengagement {
struct Candidate {
    bool eligible,member,initiating,threatened,detected,actionPending;
};
inline bool AutoJoin(const Candidate &actor) {
    if (!actor.eligible) return false;
    if (actor.member || actor.initiating) return true;
    return !actor.actionPending && (actor.threatened || actor.detected);
}
enum class JoinSelection {Keep,FreeActors,CurrentTurn};
inline JoinSelection SelectionAfterJoin(bool joinedSelected,bool hasFreeSelected) {
    if (!joinedSelected) return JoinSelection::Keep;
    return hasFreeSelected ? JoinSelection::FreeActors:JoinSelection::CurrentTurn;
}
inline bool KeepSelection(bool control,bool selectedFree,bool newActorFree) {
    return control && selectedFree && newActorFree;
}

// Native hidden perception includes opposed rolls. Reusing that result for one
// D20 round prevents a 250ms render pulse from rolling Spot/Listen repeatedly.
template<class Id> class DetectionCache {
    struct Result {uint32_t time,signature;bool detected;};
    std::map<std::pair<Id,Id>,Result> results;
public:
    template<class Sense> bool Observe(Id observer,Id actor,uint32_t now,uint32_t signature,Sense sense) {
        auto key=std::make_pair(observer,actor);
        auto found=results.find(key);
        if (found!=results.end() && found->second.signature==signature &&
            uint32_t(now-found->second.time)<6000) return found->second.detected;
        bool detected=sense();
        results[key]={now,signature,detected};
        return detected;
    }
    void Forget(Id observer,Id actor) {results.erase(std::make_pair(observer,actor));}
    void Clear() {results.clear();}
};
}
