

#include "stdafx.h"
#include "raycast.h"
#include "util/fixes.h"
#include "gamesystems/gamesystems.h"
#include "gamesystems/map/sector.h"
#include "graphics/mapterrain.h"

// Only the synchronous native query sees extra sector bits. Holding the sector
// locks prevents eviction; original flags are restored before releasing locks.
class ScopedOriginalDoorRaycast {
public:
	ScopedOriginalDoorRaycast(const RaycastPacket &packet, bool shortRange) {
		if (!gameSystems) return;
		const auto &tiles = gameSystems->GetTerrain().GetOriginalDoorBlockingTiles();
		if (tiles.empty()) return;
		auto to = shortRange ? packet.origin.location : packet.targetLoc.location;
		auto radius = (packet.flags & HasRadius) && std::isfinite(packet.radius) ? (std::max)(0.0f, packet.radius) : 0.0f;
		auto margin = int(std::ceil(radius / 28.2842712474619)) + 8;
		auto left = int64_t((std::min)(packet.origin.location.locx, to.locx)) - margin;
		auto right = int64_t((std::max)(packet.origin.location.locx, to.locx)) + margin;
		auto top = int64_t((std::min)(packet.origin.location.locy, to.locy)) - margin;
		auto bottom = int64_t((std::max)(packet.origin.location.locy, to.locy)) + margin;
		try {
			for (const auto &tile : tiles) {
				if (tile.x < left || tile.x > right || tile.y < top || tile.y > bottom) continue;
				SectorLoc loc(locXY{ static_cast<uint32_t>(tile.x), static_cast<uint32_t>(tile.y) });
				auto found = mSectors.find(loc.raw);
				if (found == mSectors.end()) {
					Sector *sector = nullptr;
					if (!sectorSys.SectorLock(loc, &sector)) throw std::runtime_error("cannot lock original door navigation sector");
					try { found = mSectors.emplace(loc.raw, std::make_pair(loc, sector)).first; }
					catch (...) { sectorSys.SectorUnlock(loc); throw; }
				}
				auto &flags = found->second.second->tilePkt.tiles[(tile.y % 64) * 64 + tile.x % 64].flags;
				mMasks.Add(flags, tile.mask);
			}
		} catch (...) { Restore(); throw; }
	}
	~ScopedOriginalDoorRaycast() { Restore(); }
private:
	std::map<uint64_t, std::pair<SectorLoc, Sector*>> mSectors;
	// Explicit reset happens before unlock, including constructor failures.
	TemporaryDoorTileMasks<TileFlags> mMasks;
	void Restore() {
		// Move is deliberately unnecessary: a nested scope owns its masks until
		// destruction. Use explicit restoration while sector pointers are valid.
		mMasks.Restore();
		for (const auto &sector : mSectors) sectorSys.SectorUnlock(sector.second.first);
		mSectors.clear();
	}
};

static class RaycastHooks : public TempleFix {
public:

	void apply() override {
		static auto originalRaycast = replaceFunction<int(__cdecl)(RaycastPacket*)>(0x100BACE0, [](RaycastPacket *packet) {
			ScopedOriginalDoorRaycast overlay(*packet, false);
			return originalRaycast(packet);
		});
		static auto originalShortRange = replaceFunction<int(__cdecl)(RaycastPacket*)>(0x100BC750, [](RaycastPacket *packet) {
			ScopedOriginalDoorRaycast overlay(*packet, true);
			return originalShortRange(packet);
		});

		// screen_raycast
		replaceFunction<int(int, int, objHndl *, GameRaycastFlags)>(0x10022360, [](int x, int y, objHndl *handleOut, GameRaycastFlags flags) {
			return PickObjectOnScreen(x, y, handleOut, flags) ? 1 : 0;
		});

	}

} hooks;
