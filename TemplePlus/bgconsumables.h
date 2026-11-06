#pragma once
inline bool BgAutoIdentifyConsumable(bool convertedScene, bool scroll, bool potion) {
    return convertedScene && (scroll || potion);
}
