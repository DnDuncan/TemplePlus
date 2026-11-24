#pragma once
#include "originaldoorstate.h"

// Native inventory objects back baked Infinity scenery. No inventory is stored
// here; the source polygon only supplies native picking and a hover frame.
constexpr int SourceContainerProxyMarker = 0x42474350;
struct SourceContainerRegion { int mapId; std::string id, objectId; OriginalDoorPolygon outline; };
class SourceContainerRegions {
public:
	void Bind(SourceContainerRegion region) {
		if (region.mapId<=0 || region.objectId.empty() || region.objectId.size()>128)
			throw std::invalid_argument("invalid source container identity");
		region.id=OriginalDoorState::NormalizeId(region.id);ValidateDoorPolygon(region.outline);
		mRegions[{region.mapId,region.id}]=std::move(region);
	}
	const SourceContainerRegion* Hit(int mapId,int x,int y) const {
		const SourceContainerRegion* result=nullptr;
		for (const auto &entry:mRegions) if (entry.first.first==mapId && DoorPolygonContains(entry.second.outline,x,y)) {
			if (result) return nullptr; // ambiguous source polygons never choose arbitrary loot
			result=&entry.second;
		}
		return result;
	}
	void Clear() {mRegions.clear();}
	const std::map<OriginalDoorState::Key,SourceContainerRegion> &Regions() const {return mRegions;}
private:
	std::map<OriginalDoorState::Key,SourceContainerRegion> mRegions;
};

struct objHndl;
bool IsSourceContainerProxy(objHndl handle);
