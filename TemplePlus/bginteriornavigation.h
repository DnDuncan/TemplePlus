#pragma once
#include <cmath>

// Only AR2617, in its existing exclusive native sector. No global radius change.
inline bool BgUpperFloorCentreNavigation(int scene, int sx, int sy) {
    return scene == 5191 && sx == 8 && sy == 12;
}

inline bool BgStaticClearancePassable(bool centreNavigation, float clearance, float radius) {
    if (!centreNavigation) return !(clearance < radius); // preserve native comparison
    return std::isfinite(clearance) && clearance > 0;
}
