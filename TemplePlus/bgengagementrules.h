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

// A party enemy notices a hero within 40 feet. An open hero is seen with line of
// sight, otherwise heard with Listen against DC 10. A sneaking hero opposes Spot
// with Hide and Listen with Move Silently; Spot and Listen lose 1 per 10 feet and
// night adds to Hide. Ties go to the hero.
constexpr double DetectionRangeFeet=40.0;
constexpr int NightHideBonus=5;
struct Senses {
    double distanceFeet;
    bool sight,sneaking,night;
    int spot,listen,hide,moveSilently;
    int spotRoll,listenRoll,hideRoll,moveRoll;
};
inline bool Detected(const Senses &senses) {
    if (senses.distanceFeet>DetectionRangeFeet) return false;
    int penalty=int(senses.distanceFeet/10.0);
    int listen=senses.listenRoll+senses.listen-penalty;
    if (!senses.sneaking) return senses.sight || listen>=10;
    int hide=senses.hideRoll+senses.hide+(senses.night ? NightHideBonus:0);
    if (senses.sight && senses.spotRoll+senses.spot-penalty>hide) return true;
    return listen>senses.moveRoll+senses.moveSilently;
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
