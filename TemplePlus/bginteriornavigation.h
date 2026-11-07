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

// Emit all proven A* steps, preserving turns, with the precise click as last node.
// Validate capacity before emitting anything: no partial route on overflow.
template<class Emit> bool BgEmitInteriorSubtileRoute(int steps, int capacity, Emit emit) {
    if (steps<=0 || steps>capacity) return false;
    for (int i=1;i<=steps;++i) emit(i,i==steps);
    return true;
}

// Animation slots own result metadata after the route payload. Do not copy it.
template<class Route, class Destination, class Source>
void BgCopyPreparedRoute(Destination &destination, const Source &source) {
    static_cast<Route&>(destination)=static_cast<const Route&>(source);
}
