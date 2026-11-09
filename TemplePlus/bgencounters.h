#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace bgencounters {
// Native ToEE owns exactly one displayed encounter. Every other encounter is
// parked here with its own membership and turn state until it is shown again.
// An actor belongs to at most one encounter; the logical scene only filters who
// may start or join one and is never the encounter identity.
template<class Actor,class Snapshot> class Registry {
public:
    struct Parked {uint32_t id;std::vector<Actor> members;Snapshot state;};
    uint32_t Open() {return ++last;}
    bool Owns(Actor actor) const {return Find(actor)!=parked.end();}
    uint32_t Owner(Actor actor) const {auto found=Find(actor);return found==parked.end() ? 0:found->id;}
    // Rejects an empty encounter and any overlap, so membership cannot fork.
    bool Park(uint32_t id,const std::vector<Actor> &members,const Snapshot &state) {
        if (!id || members.empty() || Has(id)) return false;
        for (auto actor:members) if (actor==Actor{} || Owns(actor)) return false;
        parked.push_back(Parked{id,members,state});
        return true;
    }
    bool Take(uint32_t id,Parked &out) {
        auto found=std::find_if(parked.begin(),parked.end(),[&](const Parked &entry) {return entry.id==id;});
        if (found==parked.end()) return false;
        out=*found;parked.erase(found);
        return true;
    }
    // The most recently parked encounter resumes when the displayed one ends.
    bool TakeLatest(Parked &out) {return !parked.empty() && Take(parked.back().id,out);}
    void Forget(Actor actor) {
        for (auto &entry:parked) entry.members.erase(std::remove(entry.members.begin(),entry.members.end(),actor),entry.members.end());
    }
    template<class Visit> void ForEach(Visit visit) const {for (const auto &entry:parked) visit(entry);}
    size_t Size() const {return parked.size();}
    void Clear() {parked.clear();}
private:
    typename std::vector<Parked>::const_iterator Find(Actor actor) const {
        return std::find_if(parked.begin(),parked.end(),[&](const Parked &entry) {
            return std::find(entry.members.begin(),entry.members.end(),actor)!=entry.members.end();});
    }
    bool Has(uint32_t id) const {
        return std::any_of(parked.begin(),parked.end(),[&](const Parked &entry) {return entry.id==id;});
    }
    std::vector<Parked> parked;
    uint32_t last=0;
};

// Native action, simultaneous-action, interrupt and picker state is global.
// It is only consistent to park an encounter while it waits for a hero's input.
struct Wait {bool heroTurn,performing,simultaneous,interrupted,picker,dialog;};
inline bool CanPark(const Wait &wait) {
    return wait.heroTurn && !wait.performing && !wait.simultaneous && !wait.interrupted && !wait.picker && !wait.dialog;
}
}
