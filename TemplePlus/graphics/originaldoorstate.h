#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <cstdint>
#include "originaldoornavigation.h"

struct OriginalDoorPoint { int x, y; };
using OriginalDoorPolygon = std::vector<OriginalDoorPoint>;
struct OriginalDoorOutline { OriginalDoorPolygon closed, open; };

inline void ValidateDoorPolygon(const OriginalDoorPolygon &polygon) {
	if (polygon.size() < 3 || polygon.size() > 256) throw std::invalid_argument("door outline requires 3..256 vertices");
	int64_t area = 0;
	for (size_t i = 0; i < polygon.size(); ++i) {
		const auto &a = polygon[i];
		const auto &b = polygon[(i + 1) % polygon.size()];
		if (a.x < 0 || a.x >= 66 * 256 || a.y < 0 || a.y >= 71 * 256)
			throw std::invalid_argument("door outline is outside terrain canvas");
		area += static_cast<int64_t>(a.x) * b.y - static_cast<int64_t>(b.x) * a.y;
	}
	if (!area) throw std::invalid_argument("door outline has zero area");
}

inline bool DoorPolygonContains(const OriginalDoorPolygon &polygon, int x, int y) {
	bool inside = false;
	for (size_t i = 0; i < polygon.size(); ++i) {
		const auto &a = polygon[i];
		const auto &b = polygon[(i + 1) % polygon.size()];
		auto cross = (static_cast<int64_t>(x) - a.x) * (b.y - a.y)
			- (static_cast<int64_t>(y) - a.y) * (b.x - a.x);
		if (!cross && x >= (std::min)(a.x, b.x) && x <= (std::max)(a.x, b.x)
			&& y >= (std::min)(a.y, b.y) && y <= (std::max)(a.y, b.y)) return true;
		if ((a.y > y) != (b.y > y) && x < a.x + (static_cast<double>(b.x) - a.x) * (y - a.y) / (b.y - a.y))
			inside = !inside;
	}
	return inside;
}

// Pixel-aligned source-art patches, independent of ToEE portal meshes and
// independent of day/night. No resources are bundled with this feature.
struct OriginalDoorPatch {
	int mapId;
	std::string id;
	int x, y, width, height; // terrain-canvas pixels, not locXY or viewport pixels
	std::array<std::string, 4> textures; // day closed/open, night closed/open
	bool open;
};

class OriginalDoorState {
public:
	static std::string NormalizeId(std::string id) {
		if (id.empty() || id.size() > 32) {
			throw std::invalid_argument("original door ID must have 1..32 ASCII characters");
		}
		for (auto &c : id) {
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '_')) {
				throw std::invalid_argument("invalid original door ID");
			}
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		}
		return id;
	}

	static void Validate(OriginalDoorPatch &patch) {
		patch.id = NormalizeId(patch.id);
		if (patch.mapId <= 0 || patch.width <= 0 || patch.height <= 0 ||
			patch.width > 1024 || patch.height > 1024 || patch.x < 0 || patch.y < 0 ||
			patch.x > 66 * 256 - patch.width || patch.y > 71 * 256 - patch.height) {
			throw std::invalid_argument("original door rectangle is outside the terrain canvas");
		}
		for (auto &path : patch.textures) {
			std::replace(path.begin(), path.end(), '\\', '/');
			if (path.size() > 240 || path.compare(0, 18, "art/bg2toee/doors/") != 0 ||
				path.find("..") != std::string::npos || path.find(':') != std::string::npos ||
				path.find("//") != std::string::npos || path.size() < 4 ||
				path.compare(path.size() - 4, 4, ".png") != 0) {
				throw std::invalid_argument("original door textures must be PNG files under art/bg2toee/doors/");
			}
		}
	}

	void Register(OriginalDoorPatch patch) {
		Validate(patch);
		auto key = std::make_pair(patch.mapId, patch.id);
		auto existing = mPatches.find(key);
		if (existing != mPatches.end()) {
			// Revisiting a map may re-register assets; keep its live state.
			patch.open = existing->second.open;
		} else {
			auto count = std::count_if(mPatches.begin(), mPatches.end(), [&](const Entry &entry) {
				return entry.first.first == patch.mapId;
			});
			if (count >= 128) {
				throw std::invalid_argument("too many original door patches on one map");
			}
		}
		mPatches[key] = std::move(patch);
	}

	const OriginalDoorPatch *Find(int mapId, const std::string &id) const {
		auto it = mPatches.find(std::make_pair(mapId, NormalizeId(id)));
		return it == mPatches.end() ? nullptr : &it->second;
	}

	void SetOpen(int mapId, const std::string &id, bool open) {
		auto it = mPatches.find(std::make_pair(mapId, NormalizeId(id)));
		if (it == mPatches.end()) {
			throw std::invalid_argument("original door is not registered on this map");
		}
		it->second.open = open;
		++mNavigationRevision;
	}

	void ClearMap(int mapId) {
		for (auto it = mNavigation.begin(); it != mNavigation.end();) {
			if (it->first.first == mapId) it = mNavigation.erase(it);
			else ++it;
		}
		++mNavigationRevision;
		for (auto it = mOutlines.begin(); it != mOutlines.end();) {
			if (it->first.first == mapId) it = mOutlines.erase(it);
			else ++it;
		}
		for (auto it = mPatches.begin(); it != mPatches.end();) {
			if (it->first.first == mapId) it = mPatches.erase(it);
			else ++it;
		}
	}

	void Bind(int mapId, const std::string &id, OriginalDoorOutline outline) {
		if (!Find(mapId, id)) throw std::invalid_argument("register door textures before binding outlines");
		ValidateDoorPolygon(outline.closed);
		ValidateDoorPolygon(outline.open);
		mOutlines[std::make_pair(mapId, NormalizeId(id))] = std::move(outline);
	}
	void Unbind(int mapId, const std::string &id) { mOutlines.erase(std::make_pair(mapId, NormalizeId(id))); }
	bool HasBindings(int mapId) const {
		for (const auto &entry : mOutlines) if (entry.first.first == mapId) return true;
		return false;
	}
	const OriginalDoorPolygon *Outline(int mapId, const std::string &id) const {
		auto patch = Find(mapId, id);
		auto found = mOutlines.find(std::make_pair(mapId, NormalizeId(id)));
		if (!patch || found == mOutlines.end()) return nullptr;
		return patch->open ? &found->second.open : &found->second.closed;
	}
	std::string HitTest(int mapId, int x, int y) const {
		std::string hit;
		for (const auto &entry : mOutlines) {
			if (entry.first.first != mapId) continue;
			auto polygon = Outline(mapId, entry.first.second);
			if (polygon && DoorPolygonContains(*polygon, x, y)) {
				if (!hit.empty()) return {}; // Ambiguous overlap must not pick an arbitrary door.
				hit = entry.first.second;
			}
		}
		return hit;
	}
	void BindNavigation(int mapId, const std::string &id, DoorNavigationGeometry geometry) {
		if (!Find(mapId, id)) throw std::invalid_argument("register door before binding navigation");
		ValidateDoorNavigation(geometry.closed); ValidateDoorNavigation(geometry.open);
		if (geometry.closed.empty()) throw std::invalid_argument("closed door requires navigation blockers");
		mNavigation[std::make_pair(mapId, NormalizeId(id))] = std::move(geometry);
		++mNavigationRevision;
	}
	void UnbindNavigation(int mapId, const std::string &id) {
		mNavigation.erase(std::make_pair(mapId, NormalizeId(id))); ++mNavigationRevision;
	}
	bool HasNavigation(int mapId, const std::string &id) const {
		return mNavigation.count(std::make_pair(mapId, NormalizeId(id))) != 0;
	}
	bool CanTravel(int mapId, const std::string &id) const {
		auto door = Find(mapId, id);
		return door && HasNavigation(mapId, id) && door->open;
	}
	const std::vector<DoorNavigationTile> &NavigationTiles(int mapId) const {
		if (mapId == mNavigationCacheMap && mNavigationCacheRevision == mNavigationRevision) return mNavigationCache;
		std::map<std::pair<int, int>, uint32_t> combined;
		for (const auto &entry : mNavigation) {
			if (entry.first.first != mapId) continue;
			auto door = Find(mapId, entry.first.second);
			if (!door) continue;
			for (const auto &tile : door->open ? entry.second.open : entry.second.closed)
				combined[std::make_pair(tile.x, tile.y)] |= tile.mask;
		}
		mNavigationCache.clear();
		for (const auto &tile : combined) mNavigationCache.push_back({tile.first.first, tile.first.second, tile.second});
		mNavigationCacheMap = mapId; mNavigationCacheRevision = mNavigationRevision;
		return mNavigationCache;
	}
	void Clear() { mPatches.clear(); mOutlines.clear(); mNavigation.clear(); ++mNavigationRevision; }
	using Key = std::pair<int, std::string>;
	using Entry = std::pair<const Key, OriginalDoorPatch>;
	const std::map<Key, OriginalDoorPatch> &Patches() const { return mPatches; }
	const std::map<Key, OriginalDoorOutline> &Outlines() const { return mOutlines; }
	const std::map<Key, DoorNavigationGeometry> &NavigationBindings() const { return mNavigation; }
	static std::size_t TextureIndex(bool open, bool night) { return (night ? 2u : 0u) + (open ? 1u : 0u); }

private:
	std::map<Key, OriginalDoorPatch> mPatches;
	std::map<Key, OriginalDoorOutline> mOutlines;
	std::map<Key, DoorNavigationGeometry> mNavigation;
	uint64_t mNavigationRevision = 0;
	mutable uint64_t mNavigationCacheRevision = 0;
	mutable int mNavigationCacheMap = -1;
	mutable std::vector<DoorNavigationTile> mNavigationCache;
};

// Capture only a press that starts on a door; never turn a ground/party press
// into a door click on release. Coordinates are terrain-canvas pixels so a
// camera shift during the gesture also cancels it. Threshold: 4 native pixels.
class OriginalDoorMousePress {
public:
	enum class Event { Down, Move, Up };
	struct Result { bool consumed; std::string toggle; };
	void Cancel() { mId.clear(); mDragged = false; }
	Result Process(int mapId, const std::string &hit, Event event, int x, int y) {
		if (event == Event::Down) {
			Cancel();
			if (hit.empty()) return { false, {} };
			mId = hit; mMap = mapId; mX = x; mY = y;
			return { true, {} };
		}
		if (mId.empty()) return { false, {} };
		if (mMap != mapId) { Cancel(); return { true, {} }; }
		auto dx = static_cast<int64_t>(x) - mX, dy = static_cast<int64_t>(y) - mY;
		if (dx < -4 || dx > 4 || dy < -4 || dy > 4) mDragged = true;
		if (event == Event::Up) {
			auto id = (!mDragged && hit == mId) ? mId : std::string();
			Cancel();
			return { true, id };
		}
		return { true, {} };
	}
private:
	std::string mId;
	int mMap = 0, mX = 0, mY = 0;
	bool mDragged = false;
};
