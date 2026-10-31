
#include "stdafx.h"
#include "originaldoorsave.h"
#include "mapterrain.h"
#include "maps.h"
#include <tig/tig.h>
#include <graphics/device.h>
#include <graphics/shaperenderer2d.h>
#include <infrastructure/mesparser.h>
#include <infrastructure/vfs.h>
#include <config/config.h>
#include <util/fixes.h>
#include <gamesystems/gamesystems.h>
#include <gamesystems/timeevents.h>
#include "gameview.h"
#include "pathfinding.h"
#include "path_node.h"
#include "gamesystems/objects/objsystem.h"
#include "gamesystems/map/sector.h"
#include "gamesystems/mapsystem.h"
#include "animgoals/anim.h"
#include "combat.h"
#include "critter.h"
#include "party.h"
#include "raycast.h"

TerrainSystem *originalDoorTerrain = nullptr;

TerrainSystem::TerrainSystem(gfx::RenderingDevice& device, gfx::ShapeRenderer2d& shapeRenderer)
	: mDevice(device),mShapeRenderer(shapeRenderer) {

	// Previously initialized in ground_init
	mTerrainTintRed = 1.0f;
	mTerrainTintGreen = 1.0f;
	mTerrainTintBlue = 1.0f;
	originalDoorTerrain = this;

}

TerrainSystem::~TerrainSystem() { originalDoorTerrain = nullptr; }

void TerrainSystem::Render() {

	gfx::PerfGroup perfGroup(mDevice, "Terrain");

	// Special dirty case for the 5000 map which has no terrain
	if (maps.GetCurrentMapId() == 5000) {
		return;
	}

	// Check the day<->night transition and stop if necessary
	if (mIsTransitioning) {
		auto timeSinceMapEntered = timeGetTime() - mTransitionStart;
		if (timeSinceMapEntered > TransitionTime) {
			mIsTransitioning = false;
		}

		mTransitionProgress = timeSinceMapEntered / (float)TransitionTime;
	}

	auto viewport = gameView->GetWorldViewport();

	// The terrain is centered on the center tile of the map
	// This is 480,480 for all vanilla maps, but theoretically it 
	// depends on the map size specified in the map's map.prp file
	// The center of the map in pixels relative to the current screen viewport
	int terrainOriginX, terrainOriginY;
	GetTerrainOrigin(terrainOriginX, terrainOriginY);

	auto startX = (std::max)(0, int(std::floor((viewport.left - terrainOriginX) / TileSize)));
	auto startY = (std::max)(0, int(std::floor((viewport.top - terrainOriginY) / TileSize)));

	for (auto y = startY; y < MapHeightTiles; ++y) {
		TigRect destRect;
		destRect.y = terrainOriginY + y * TileSize;
		destRect.height = TileSize;
		for (auto x = startX; x < MapWidthTiles; ++x) {
			destRect.x = terrainOriginX + x * TileSize;
			destRect.width = TileSize;

			// Get the correct texture for this tile
			RenderTile(x, y, destRect);

			// The next column would be out of view
			if (destRect.x + TileSize >= viewport.right) {
				break;
			}
		}

		// The next column would be out of view
		if (destRect.y + TileSize >= viewport.bottom) {
			break;
		}
	}

	// Source-art patches are part of terrain: draw before actors, never as a
	// substituted 3D portal and never by rewriting the terrain JPEG files.
	RenderOriginalDoors(terrainOriginX, terrainOriginY);
}

void TerrainSystem::RegisterOriginalDoor(OriginalDoorPatch patch) {
	OriginalDoorState::Validate(patch);
	if (patch.mapId != maps.GetCurrentMapId()) {
		throw std::invalid_argument("register original doors only on the current map");
	}
	ValidateOriginalDoorTextures(patch);
	mOriginalDoors.Register(std::move(patch));
	++mDoorUseRevision;
}

void TerrainSystem::ValidateOriginalDoorTextures(const OriginalDoorPatch &patch) const {
	// Resolve every state before changing the registry, so a missing asset does
	// not replace a previously valid patch or silently hide the background.
	for (const auto &path : patch.textures) {
		auto texture = mDevice.GetTextures().Resolve(path, false);
		if (!texture || !texture->IsValid() || !texture->GetResourceView()) {
			throw std::invalid_argument("original door texture could not be loaded: " + path + "; see TemplePlus.log");
		}
		const auto &size = texture->GetSize();
		if (size.width != patch.width || size.height != patch.height) {
			throw std::invalid_argument(fmt::format("original door texture {} is {}x{}; expected {}x{}",
				path, size.width, size.height, patch.width, patch.height));
		}
	}
}

void TerrainSystem::SetOriginalDoorOpen(const std::string &id, bool open) {
	auto candidate = mOriginalDoors;
	candidate.SetOpen(maps.GetCurrentMapId(), id, open);
	CheckNewDoorBlockers(candidate);
	mOriginalDoors = std::move(candidate);
	++mDoorUseRevision;
	pathfindingSys.PathCacheInit();
}

void TerrainSystem::CheckNewDoorBlockers(const OriginalDoorState &candidate) const {
	auto mapId = maps.GetCurrentMapId();
	std::map<std::pair<int, int>, uint32_t> before;
	for (const auto &tile : mOriginalDoors.NavigationTiles(mapId)) before[{tile.x, tile.y}] = tile.mask;
	std::vector<DoorNavigationTile> added;
	for (auto tile : candidate.NavigationTiles(mapId)) {
		tile.mask &= ~before[{tile.x, tile.y}];
		LocAndOffsets loc{ {static_cast<uint32_t>(tile.x), static_cast<uint32_t>(tile.y)}, 0, 0 };
		tile.mask &= ~static_cast<uint32_t>(sectorSys.GetTileFlags(loc)); // existing walls are not new blockers
		if (tile.mask) added.push_back(tile);
	}
	if (added.empty()) return;
	bool occupied = false;
	gameSystems->GetObj().ForEachObj([&](objHndl handle, GameObjectBody &obj) {
		if (occupied || !obj.IsCritter() || (obj.GetFlags() & (OF_OFF | OF_DESTROYED | OF_NO_BLOCK))) return;
		auto pos = obj.GetLocationFull().ToInches3D();
		occupied = DoorNavigationBlocksSegment(added, pos.x, pos.z, pos.x, pos.z, objects.GetRadius(handle) * 0.7);
	});
	if (occupied) throw std::invalid_argument("door movement blocked by an actor; move away from the leaf first");
}

void TerrainSystem::BindOriginalDoorNavigation(const std::string &id, DoorNavigationGeometry geometry) {
	if (PathNodeSys::hasClearanceData) throw std::invalid_argument("door navigation probe requires a map without precomputed clearance");
	ValidateDoorNavigation(geometry.closed); ValidateDoorNavigation(geometry.open);
	for (const auto *state : {&geometry.closed, &geometry.open}) for (const auto &tile : *state) {
		SectorLoc loc(locXY{ static_cast<uint32_t>(tile.x), static_cast<uint32_t>(tile.y) });
		Sector *sector = nullptr;
		if (!sectorSys.SectorLock(loc, &sector)) throw std::invalid_argument("door navigation requires existing map sectors");
		sectorSys.SectorUnlock(loc);
	}
	auto candidate = mOriginalDoors;
	candidate.BindNavigation(maps.GetCurrentMapId(), id, std::move(geometry));
	CheckNewDoorBlockers(candidate);
	mOriginalDoors = std::move(candidate);
	++mDoorUseRevision;
	pathfindingSys.PathCacheInit();
}
void TerrainSystem::UnbindOriginalDoorNavigation(const std::string &id, int mapId) {
	mapId = mapId ? mapId : maps.GetCurrentMapId();
	mOriginalDoors.UnbindNavigation(mapId, id); UnbindOriginalDoorUse(mapId, id); pathfindingSys.PathCacheInit();
}
bool TerrainSystem::HasOriginalDoorNavigation(const std::string &id) const {
	return mOriginalDoors.HasNavigation(maps.GetCurrentMapId(), id);
}
bool TerrainSystem::CanTravelThroughOriginalDoor(int mapId, const std::string &id) const {
	return mOriginalDoors.CanTravel(mapId, id);
}
const std::vector<DoorNavigationTile> &TerrainSystem::GetOriginalDoorBlockingTiles() const {
	return mOriginalDoors.NavigationTiles(maps.GetCurrentMapId());
}

void TerrainSystem::BindOriginalDoorUse(int mapId, const std::string &id, OriginalDoorUseGeometry geometry) {
	ValidateOriginalDoorUse(geometry);
	if (!mOriginalDoors.Find(mapId, id) || !mOriginalDoors.HasNavigation(mapId, id))
		throw std::invalid_argument("register door and navigation before binding approach");
	mDoorUse[{mapId, OriginalDoorState::NormalizeId(id)}] = std::move(geometry);
	CancelOriginalDoorUse(); ++mDoorUseRevision;
}

void TerrainSystem::UnbindOriginalDoorUse(int mapId, const std::string &id) {
	mDoorUse.erase({mapId, OriginalDoorState::NormalizeId(id)});
	CancelOriginalDoorUse(); ++mDoorUseRevision;
}

bool TerrainSystem::HasOriginalDoorUse(int mapId, const std::string &id) const {
	return mDoorUse.count({mapId, OriginalDoorState::NormalizeId(id)}) != 0;
}

void TerrainSystem::RequestOriginalDoorUse(const std::string &id, uint64_t actorHandle) {
	objHndl actor{actorHandle};
	if (!gameSystems->GetObj().IsValidHandle(actor) || !party.IsInParty(actor)
		|| critterSys.IsDeadOrUnconscious(actor) || combatSys.isCombatActive())
		throw std::invalid_argument("door approach requires a conscious party actor outside combat");
	auto mapId = maps.GetCurrentMapId();
	auto found = mDoorUse.find({mapId, OriginalDoorState::NormalizeId(id)});
	if (found == mDoorUse.end()) throw std::invalid_argument("original door approach is not bound");
	auto from = objects.GetLocationFull(actor).ToInches2D();
	mDoorUseQueue.Begin(mapId, found->first.second, actorHandle, mDoorUseRevision,
		timeGetTime(), GetOriginalDoorOpen(id), found->second, {from.x, from.y});
	AdvanceTime(timeGetTime()); // near use is immediate, but still requires a clear line
	if (mDoorUseQueue.Pending()) {
		auto target = mDoorUseQueue.Pending()->target;
		if (!gameSystems->GetAnim().PushMoveToTile(actor, LocAndOffsets::FromInches(float(target.x), float(target.y)))) {
			CancelOriginalDoorUse(); throw std::invalid_argument("cannot start movement to original door approach point");
		}
	}
}

void TerrainSystem::AdvanceTime(uint32_t time) {
	if (!mDoorUseQueue.Pending()) return;
	try {
		auto request = *mDoorUseQueue.Pending();
		if (gameSystems->GetMap().IsClearingMap() || maps.GetCurrentMapId() != request.mapId
			|| request.revision != mDoorUseRevision) { CancelOriginalDoorUse(); return; }
		objHndl actor{request.actor};
		auto valid = gameSystems->GetObj().IsValidHandle(actor) && party.IsInParty(actor)
			&& !critterSys.IsDeadOrUnconscious(actor);
		bool reachable = false;
		if (valid && !combatSys.isCombatActive()) {
			auto from = objects.GetLocationFull(actor);
			auto inches = from.ToInches2D();
			if (OriginalDoorUseDistance({inches.x, inches.y}, request.target) <= request.reach) {
				RaycastPacket ray;
				ray.flags = static_cast<RaycastFlags>(HasToBeCleared | HasSourceObj | ExcludeItemObjects);
				ray.origin = from; ray.targetLoc = LocAndOffsets::FromInches(float(request.target.x), float(request.target.y));
				ray.sourceObj = actor; ray.radius = 0;
				ray.Raycast(); reachable = true;
				for (int i = 0; i < ray.resultCount; ++i)
					if (ray.results[i].flags & BlockerSubtile) { reachable = false; break; }
			}
		}
		auto action = mDoorUseQueue.Poll(maps.GetCurrentMapId(), mDoorUseRevision, time, valid,
			combatSys.isCombatActive(), GetOriginalDoorOpen(request.id), reachable);
		if (action == OriginalDoorUseQueue::Action::Apply) {
			SetOriginalDoorOpen(request.id, request.open);
			logger->info("Original door approach applied: map={} id={} open={}", request.mapId, request.id, request.open);
		} else if (action == OriginalDoorUseQueue::Action::Cancelled) {
			logger->info("Original door approach cancelled: map={} id={}", request.mapId, request.id);
		}
	} catch (const std::exception &e) {
		CancelOriginalDoorUse(); logger->info("Original door approach refused: {}", e.what());
	}
}

bool TerrainSystem::GetOriginalDoorOpen(const std::string &id) const {
	return GetOriginalDoorOpenOnMap(maps.GetCurrentMapId(), id);
}

bool TerrainSystem::GetOriginalDoorOpenOnMap(int mapId, const std::string &id) const {
	auto patch = mOriginalDoors.Find(mapId, id);
	if (!patch) throw std::invalid_argument("original door is not registered on this map");
	return patch->open;
}

void TerrainSystem::SaveOriginalDoorSession(const std::string &metadata) const {
	auto bytes = originaldoorsave::Encode(mOriginalDoors, metadata);
	vfs->WriteBinaryFile("Save/Current/bg2toee-doors-v1.bin", gsl::span<uint8_t>(bytes.data(), bytes.size()));
}

std::string TerrainSystem::LoadOriginalDoorSession() {
	Reset(); // old saves and rejected sidecars cannot inherit another game's doors
	pathfindingSys.PathCacheInit();
	const char *path = "Save/Current/bg2toee-doors-v1.bin";
	if (!vfs->FileExists(path)) return {};
	auto file = vfs->Open(path, "rb");
	if (!file) throw std::invalid_argument("cannot open original door save");
	auto size = vfs->Length(file); vfs->Close(file);
	if (size > originaldoorsave::MaxBytes) throw std::invalid_argument("original door save exceeds size limit");
	auto session = originaldoorsave::Decode(vfs->ReadAsBinary(path));
	for (const auto &entry : session.doors.Patches()) ValidateOriginalDoorTextures(entry.second);
	if (PathNodeSys::hasClearanceData && !session.doors.NavigationTiles(maps.GetCurrentMapId()).empty())
		throw std::invalid_argument("saved door navigation requires a map without precomputed clearance");
	CheckNewDoorBlockers(session.doors);
	mOriginalDoors = std::move(session.doors);
	pathfindingSys.PathCacheInit();
	return session.metadata;
}

void TerrainSystem::ClearOriginalDoors() {
	mOriginalDoors.ClearMap(maps.GetCurrentMapId());
	for (auto it = mDoorUse.begin(); it != mDoorUse.end();) {
		if (it->first.first == maps.GetCurrentMapId()) it = mDoorUse.erase(it); else ++it;
	}
	CancelOriginalDoorUse(); ++mDoorUseRevision;
	pathfindingSys.PathCacheInit();
	mHoveredOriginalDoor.clear();
}

void TerrainSystem::GetTerrainOrigin(int &x, int &y) const {
	auto center = maps.GetMapCenterTile();
	locSys.ToTranslation(center.locx, center.locy, x, y);
	x -= MapWidthPixels / 2 - 40;
	y -= MapHeightPixels / 2 - 14;
}

void TerrainSystem::BindOriginalDoor(const std::string &id, OriginalDoorOutline outline) {
	mOriginalDoors.Bind(maps.GetCurrentMapId(), id, std::move(outline));
	CancelOriginalDoorUse(); ++mDoorUseRevision;
}
void TerrainSystem::UnbindOriginalDoor(const std::string &id) {
	mOriginalDoors.Unbind(maps.GetCurrentMapId(), id);
	CancelOriginalDoorUse(); ++mDoorUseRevision;
	mHoveredOriginalDoor.clear();
}
bool TerrainSystem::IsOriginalDoorBound(const std::string &id) const {
	return mOriginalDoors.Outline(maps.GetCurrentMapId(), id) != nullptr;
}
bool TerrainSystem::HasOriginalDoorInput() const {
	return mOriginalDoors.HasBindings(maps.GetCurrentMapId());
}
std::string TerrainSystem::HitTestOriginalDoor(int screenX, int screenY) const {
	if (!HasOriginalDoorInput()) return {};
	int x, y; GetTerrainOrigin(x, y);
	return mOriginalDoors.HitTest(maps.GetCurrentMapId(), screenX - x, screenY - y);
}

void TerrainSystem::Reset() {
	mOriginalDoors.Clear();
	mDoorUse.clear(); CancelOriginalDoorUse(); ++mDoorUseRevision;
	mHoveredOriginalDoor.clear();
}

void TerrainSystem::UnloadModule() {
	mOriginalDoors.Clear();
	mDoorUse.clear(); CancelOriginalDoorUse(); ++mDoorUseRevision;
	mHoveredOriginalDoor.clear();
	mTerrainDirs.clear();
	mMapArtId = 0;
}

void TerrainSystem::RenderOriginalDoors(int terrainOriginX, int terrainOriginY) {
	auto mapId = maps.GetCurrentMapId();
	auto viewport = gameView->GetWorldViewport();
	for (const auto &entry : mOriginalDoors.Patches()) {
		const auto &patch = entry.second;
		if (patch.mapId != mapId) continue;
		auto x = terrainOriginX + patch.x;
		auto y = terrainOriginY + patch.y;
		if (x + patch.width <= viewport.left || y + patch.height <= viewport.top ||
			x >= viewport.right || y >= viewport.bottom) continue;
		auto draw = [&](bool night, float alpha) {
			auto index = OriginalDoorState::TextureIndex(patch.open, night);
			auto texture = mDevice.GetTextures().Resolve(patch.textures[index], false);
			if (!texture || !texture->IsValid()) return;
			XMCOLOR color(mTerrainTintRed, mTerrainTintGreen, mTerrainTintBlue, alpha);
			mShapeRenderer.DrawRectangle(static_cast<float>(x), static_cast<float>(y),
				static_cast<float>(patch.width), static_cast<float>(patch.height),
				*texture, color, gfx::SamplerType2d::POINT);
		};
		draw(mIsTransitioning ? !mIsNightTime : mIsNightTime, 1.0f);
		if (mIsTransitioning) draw(mIsNightTime, mTransitionProgress);
	}
	if (!mHoveredOriginalDoor.empty()) {
		auto polygon = mOriginalDoors.Outline(mapId, mHoveredOriginalDoor);
		if (polygon) {
			std::vector<gfx::Line2d> lines;
			for (size_t i = 0; i < polygon->size(); ++i) {
				const auto &a = (*polygon)[i];
				const auto &b = (*polygon)[(i + 1) % polygon->size()];
				lines.emplace_back(XMFLOAT2(float(terrainOriginX + a.x), float(terrainOriginY + a.y)),
					XMFLOAT2(float(terrainOriginX + b.x), float(terrainOriginY + b.y)), XMCOLOR(1.0f, 0.85f, 0.1f, 1.0f));
			}
			mShapeRenderer.DrawLines(lines);
		}
	}
}

void TerrainSystem::RenderTile(int x, int y, const TigRect& destRect) {
	auto primaryMapArtId = mMapArtId;

	// Handling transition
	if (mIsTransitioning) {
		// This is flipped while we transition, since we have to draw 
		// the old map first
		if (!mIsNightTime) {
			primaryMapArtId += NightArtIdOffset;
		}
	} else {
		if (mIsNightTime) {
			primaryMapArtId += NightArtIdOffset;
		}
	}
	
	auto textureName = GetTileTexture(primaryMapArtId, x, y);
	auto texture = mDevice.GetTextures().Resolve(textureName, false);

	if (!texture->IsValid()) {
		return;
	}

	XMCOLOR color(mTerrainTintRed, mTerrainTintGreen, mTerrainTintBlue, 1);

	auto destX = (float)destRect.x;
	auto destY = (float)destRect.y;
	auto destWidth = (float)destRect.width;
	auto destHeight = (float)destRect.height;

	mShapeRenderer.DrawRectangle(destX, destY, destWidth, destHeight, *texture, color);

	if (mIsTransitioning) {

		// Use the real map here
		if (mIsNightTime) {
			primaryMapArtId = mMapArtId + NightArtIdOffset;
		} else {
			primaryMapArtId = mMapArtId;
		}
		textureName = GetTileTexture(primaryMapArtId, x, y);
		texture = mDevice.GetTextures().Resolve(textureName, false);

		// Draw the "new" map over the old one with alpha that is based on 
		// the time since the transition started		
		color = XMCOLOR(mTerrainTintRed, mTerrainTintGreen, mTerrainTintBlue, mTransitionProgress);

		mShapeRenderer.DrawRectangle(destX, destY, destWidth, destHeight, *texture, color);
	}

}

const std::string & TerrainSystem::GetName(void) const
{
	static std::string sName = "Terrain";
	return sName;
}

void TerrainSystem::Load(int groundArtId)
{
	mHoveredOriginalDoor.clear();
	if (mMapArtId == groundArtId) {
		return;
	}
	mMapArtId = groundArtId;
	mIsTransitioning = false;
	mIsNightTime = !gameSystems->GetTimeEvent().IsDaytime();
}

void TerrainSystem::LoadModule()
{
	auto groundMapping = MesFile::ParseFile("art/ground/ground.mes");
	for (auto& pair : groundMapping) {
		mTerrainDirs[pair.first] = fmt::format("art/ground/{}/", pair.second);
	}
}

void TerrainSystem::UpdateDayNight() {
	if (gameSystems->GetTimeEvent().IsDaytime()) {
		// Start the night -> day transition
		if (mIsNightTime) {
			mIsTransitioning = true;
			mIsNightTime = false;
			mTransitionStart = timeGetTime();
		}
	} else {
		// Start the day -> night transition
		if (!mIsNightTime) {
			mIsTransitioning = true;
			mIsNightTime = true;
			mTransitionStart = timeGetTime();
		}
	}
}

std::string TerrainSystem::GetTileTexture(int mapArtId, int x, int y) {

	// Find the directory that the JPEGs are kept in
	auto it = mTerrainDirs.find(mapArtId);

	if (it == mTerrainDirs.end()) {
		return std::string();
	}
	
	return fmt::format("{}{:04x}{:04x}.jpg", it->second, y, x);

}

static class GroundSystemHooks : public TempleFix {
public:
	void apply() override {
		// ground_add_dirty_rect
		replaceFunction<void(TigRect*)>(0x1002c4d0, [](TigRect*) {
			// Simply ignored now (no dirty tracking)
		});

		// map_ground_update_day_night
		replaceFunction<void()>(0x1002d290, []() {
			gameSystems->GetTerrain().UpdateDayNight();
		});

		// The timer event init function (100616A0) calls a function in the ground system
		// creating a cyclic dependency
		writeNoops(0x10061748);
	}

} groundHooks;
