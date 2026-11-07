#pragma once
#include <cmath>

// Only AR2617, in its existing exclusive native sector. No global radius change.
inline bool BgUpperFloorCentreNavigation(int scene, int sx, int sy) {
    return scene == 5191 && sx == 8 && sy == 12;
}

inline bool BgStaticClearancePassable(bool centreNavigation, float clearance, float radius) {
    if (!std::isfinite(clearance) || clearance <= 0) return false;
    return centreNavigation || clearance >= radius;
}
