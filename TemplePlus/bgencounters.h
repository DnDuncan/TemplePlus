#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <type_traits>
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
    template<class Visit> void ForEach(Visit visit) {for (auto &entry:parked) visit(entry);}
    size_t Size() const {return parked.size();}
    void Clear() {parked.clear();}
    uint32_t Last() const {return last;}
    // A loaded save continues the ids it was written with.
    void Seed(uint32_t id) {if (id>last) last=id;}
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

// Waiting fights are saved as plain records (Save\\Current\\bg_encounters.dat).
// The reader never reads past the data, so a truncated file is rejected.
class Writer {
public:
    template<class T> void Put(const T &value) {
        static_assert(std::is_trivially_copyable<T>::value,"plain record fields only");
        auto bytes=reinterpret_cast<const uint8_t*>(&value);
        data.insert(data.end(),bytes,bytes+sizeof(T));
    }
    const std::vector<uint8_t> &Bytes() const {return data;}
private:
    std::vector<uint8_t> data;
};
class Reader {
public:
    explicit Reader(const std::vector<uint8_t> &data):data(data) {}
    template<class T> bool Get(T &value) {
        static_assert(std::is_trivially_copyable<T>::value,"plain record fields only");
        if (data.size()-at<sizeof(T)) return false;
        std::memcpy(&value,data.data()+at,sizeof(T));at+=sizeof(T);
        return true;
    }
    bool Done() const {return at==data.size();}
private:
    const std::vector<uint8_t> &data;
    size_t at=0;
};

// A downed hero cannot act. Its turn is skipped while its fight still has an
// active hero. When every hero of the fight is down but another hero is up, the
// fight is held and goes on without them instead of waiting on that turn.
enum class DownedTurn {Act,Skip,Hold};
inline DownedTurn OnHeroTurn(bool downed,bool fightHasActiveHero,bool partyHasActiveHero) {
    if (!downed) return DownedTurn::Act;
    return !fightHasActiveHero && partyHasActiveHero ? DownedTurn::Hold:DownedTurn::Skip;
}

// A hidden hero's attack on an enemy that has not noticed it is an unseen
// attack (3.5): +2 to hit, the target loses its Dex and dodge bonuses to AC and
// sneak attack applies. It is noted when the action starts, before attacking
// reveals the hero, and holds for that action's attacks only.
template<class Actor> class Opening {
public:
    static constexpr uint32_t Lifetime=6000;
    void Note(Actor attacker,Actor target,bool unseen,uint32_t now) {
        striker=attacker;struck=target;hidden=unseen;at=now;
    }
    bool Unseen(Actor attacker,Actor target,uint32_t now) const {
        return hidden && attacker==striker && target==struck && uint32_t(now-at)<Lifetime;
    }
    void Clear() {hidden=false;}
private:
    Actor striker{},struck{};
    bool hidden=false;
    uint32_t at=0;
};

// An enemy's round in a fight whose heroes are all down (design 9.1 a): with no
// conscious hero near it, it finishes a downed hero of its fight (3.5 coup de
// grace on a helpless target) or runs to one; otherwise it holds its ground.
enum class EnemyMove {Hold,Approach,Finish};
inline EnemyMove UnattendedEnemy(bool downedTarget,bool heroNearby,bool inReach) {
    if (!downedTarget || heroNearby) return EnemyMove::Hold;
    return inReach ? EnemyMove::Finish:EnemyMove::Approach;
}

// A fight without an active hero keeps its own time: one round per 6 seconds
// while it runs. A gap between pulses (dialogue, menus, loading) counts at most
// one step, so nothing is caught up at once.
class RoundClock {
public:
    static constexpr uint32_t Period=6000,MaxStep=1000;
    void Start(uint32_t now) {last=now;elapsed=0;}
    unsigned Advance(uint32_t now,bool running) {
        auto step=(std::min)(uint32_t(now-last),MaxStep);last=now;
        if (!running) return 0;
        elapsed+=step;auto rounds=unsigned(elapsed/Period);elapsed%=Period;
        return rounds;
    }
private:
    uint32_t last=0,elapsed=0;
};
}
