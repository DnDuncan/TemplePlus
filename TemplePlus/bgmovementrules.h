#pragma once
namespace bgmovement {
// -1 preserves the native settings until the player chooses a mode.
inline bool BaseRun(int mode, bool alwaysRun, int walkDistance) {
    return mode == 0 ? false : mode == 1 ? true : alwaysRun && walkDistance == 0;
}
inline bool Run(int mode, bool alwaysRun, int walkDistance, bool ctrl, bool numLock) {
    auto base = BaseRun(mode, alwaysRun, walkDistance);
    if (ctrl) return !base;
    return base || (mode == -1 && walkDistance == 0 && numLock);
}
}
