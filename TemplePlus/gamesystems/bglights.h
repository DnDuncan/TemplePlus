#pragma once
#include <cmath>
#include <cstdint>
#include <map>

/*
	bg2toee dynamic point lights (camp fires). ToEE lights 3D objects only with
	the static lights of the map sectors; these are added by the mod's Python
	(game.bg_light_add) and gathered by MapObjectRenderer::FindLights for the
	current map. Pure data, shared with standalone tests.
*/
namespace bglights {

struct Light {
	int mapId = 0;
	float x = 0, y = 0, height = 0;   // world inches, height above the ground
	float r = 1, g = 1, b = 1;         // 0..1
	float range = 0;                   // inches
	float flicker = 0;                 // 0 none .. 1 strong
};

inline bool Valid(const Light &l) {
	return l.mapId > 0 && std::isfinite(l.x) && std::isfinite(l.y) && std::isfinite(l.height)
		&& l.range > 0 && l.range <= 2400 && l.r >= 0 && l.r <= 1 && l.g >= 0 && l.g <= 1 && l.b >= 0 && l.b <= 1
		&& l.flicker >= 0 && l.flicker <= 1;
}

// Brightness factor of a flickering fire at `ms` (deterministic, 0.55..1).
inline float Flicker(const Light &l, uint32_t ms, int id) {
	if (l.flicker <= 0) return 1.0f;
	auto t = static_cast<float>(ms) / 1000.0f, phase = static_cast<float>(id);
	auto wave = 0.5f * std::sin(t * 7.1f + phase) + 0.3f * std::sin(t * 13.7f + phase * 2.3f) + 0.2f * std::sin(t * 23.3f);
	return 1.0f - l.flicker * 0.45f * (0.5f + 0.5f * wave);
}

// Is the light close enough to light an object at (x, y) searched within `radius`?
inline bool Reaches(const Light &l, float x, float y, float radius) {
	auto dx = x - l.x, dy = y - l.y, reach = radius + l.range;
	return dx * dx + dy * dy <= reach * reach;
}

constexpr size_t MaxLights = 64;

class Registry {
public:
	int Add(const Light &l) {
		if (!Valid(l) || mLights.size() >= MaxLights) return 0;
		auto id = ++mNext;
		mLights[id] = l;
		return id;
	}
	bool Remove(int id) { return mLights.erase(id) > 0; }
	void Clear() { mLights.clear(); }
	const std::map<int, Light> &All() const { return mLights; }
private:
	std::map<int, Light> mLights;
	int mNext = 0;
};

Registry &Lights();

}
