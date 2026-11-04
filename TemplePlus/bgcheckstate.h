#pragma once
#include <cstdint>
#include <map>
#include <tuple>

// A result can be committed once, only after the animation and confirmation.
// This policy has no engine dependencies: tests exercise the real UI lifecycle.
namespace bgchecks {
class SlotBudget {
public:
    bool Reserve(uint64_t owner,uint32_t spellClass,uint32_t level,int available) {
        auto &used=mUsed[{owner,spellClass,level}];
        if (available<=used) return false;
        ++used;return true;
    }
private:
    std::map<std::tuple<uint64_t,uint32_t,uint32_t>,int> mUsed;
};
enum class Phase { Preparing, Rolling, Result, Committed, Cancelled };
struct CheckState {
    Phase phase=Phase::Preparing;
    uint32_t started=0, duration=1800;
    int natural=0, bonus=0, dc=0;
    bool Roll(uint32_t now,int die,int modifier,int difficulty) {
        if (phase!=Phase::Preparing || die<1 || die>20) return false;
        started=now;natural=die;bonus=modifier;dc=difficulty;phase=Phase::Rolling;return true;
    }
    void Advance(uint32_t now) {
        if (phase==Phase::Rolling && uint32_t(now-started)>=duration) phase=Phase::Result;
    }
    bool Cancel() {
        if (phase!=Phase::Preparing) return false;
        phase=Phase::Cancelled;return true;
    }
    bool Commit() {
        if (phase!=Phase::Result) return false;
        phase=Phase::Committed;return true;
    }
    bool Success(bool naturalOneFails) const {return !(naturalOneFails && natural==1) && natural+bonus>=dc;}
};
}
