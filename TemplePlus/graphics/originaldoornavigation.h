#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

struct DoorNavigationTile { int x, y; uint32_t mask; };
struct DoorNavigationGeometry { std::vector<DoorNavigationTile> closed, open; };

inline void ValidateDoorNavigation(const std::vector<DoorNavigationTile> &tiles) {
	if (tiles.size() > 256) throw std::invalid_argument("door navigation exceeds 256 tiles per state");
	std::map<std::pair<int, int>, bool> seen;
	for (const auto &tile : tiles) {
		if (tile.x < 0 || tile.x >= 960 || tile.y < 0 || tile.y >= 960 || !tile.mask || (tile.mask & ~0x3fe00u))
			throw std::invalid_argument("invalid door navigation tile or blocking mask");
		if (!seen.emplace(std::make_pair(tile.x, tile.y), true).second)
			throw std::invalid_argument("duplicate door navigation tile");
	}
}

// Same centers/radius as the native sector raycast. No viewport/camera units.
inline bool DoorNavigationBlocksSegment(const std::vector<DoorNavigationTile> &tiles,
	double ax, double ay, double bx, double by, double radius) {
	constexpr double subtile = 28.2842712474619 / 3;
	auto dx = bx - ax, dy = by - ay, length2 = dx * dx + dy * dy;
	auto r = (std::max)(0.0, radius) + subtile * 0.7071067811865476;
	for (const auto &tile : tiles) for (int j = 0; j < 3; ++j) for (int i = 0; i < 3; ++i) {
		if (!(tile.mask & (0x200u << (j * 3 + i)))) continue;
		auto x = (tile.x * 3 + i + 0.5) * subtile, y = (tile.y * 3 + j + 0.5) * subtile;
		auto t = length2 > 0 ? ((x - ax) * dx + (y - ay) * dy) / length2 : 0.0;
		t = (std::max)(0.0, (std::min)(1.0, t));
		auto ex = ax + t * dx - x, ey = ay + t * dy - y;
		if (ex * ex + ey * ey <= r * r) return true;
	}
	return false;
}

// Restores exact flags even for overlapping/nested queries or an exception.
// No sector dirty bits, save data, static obstacles or materials are changed.
template<class Flags> class TemporaryDoorTileMasks {
public:
	void Add(Flags &flags, uint32_t mask) {
		mBefore.emplace_back(&flags, flags); // allocate before mutating
		flags = static_cast<Flags>(static_cast<uint32_t>(flags) | mask);
	}
	void Restore() {
		for (auto it = mBefore.rbegin(); it != mBefore.rend(); ++it) *it->first = it->second;
		mBefore.clear();
	}
	~TemporaryDoorTileMasks() { Restore(); }
	TemporaryDoorTileMasks() = default;
	TemporaryDoorTileMasks(const TemporaryDoorTileMasks&) = delete;
	TemporaryDoorTileMasks& operator=(const TemporaryDoorTileMasks&) = delete;
private:
	std::vector<std::pair<Flags*, Flags>> mBefore;
};
