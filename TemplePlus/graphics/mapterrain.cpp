
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
#include "float_line.h"
#include "tig/tig_mouse.h"
#include "gamesystems/legacymapsystems.h"
#include "gamesystems/legacysystems.h"
#include "python/python_integration_obj.h"

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
	RenderSourceContainers();
}

bool IsSourceContainerProxy(objHndl handle) {
	if (!handle || !objSystem || !objSystem->IsValidHandle(handle)) return false;
	auto obj=objSystem->GetObject(handle);
	return obj->type==obj_t_container && obj->GetInt32(obj_f_container_pad_i_4)==SourceContainerProxyMarker;
}
void TerrainSystem::BindSourceContainer(SourceContainerRegion region) {
	// Validate everything before changing the backing object's presentation.
	SourceContainerRegions candidate=mSourceContainers;candidate.Bind(region);
	auto handle=objSystem->FindObjectByIdStr(region.objectId);
	if (handle) {
		auto obj=objSystem->GetObject(handle);
		// Project chests are tagged 'BGC' + index (0x42474301 is the first barrel,
		// further Candlekeep containers count up from 0x42474302).
		auto tag=static_cast<uint32_t>(obj->GetInt32(obj_f_container_pad_i_1));
		if (obj->type!=obj_t_container || (tag&0xFFFFFF00u)!=0x42474300u || (tag&0xFFu)==0
			|| obj->GetInt32(obj_f_container_pad_i_2)!=1)
			throw std::invalid_argument("source container GUID is not the initialized project chest");
		obj->SetInt32(obj_f_container_pad_i_4,SourceContainerProxyMarker);
		objects.SetFlag(handle,static_cast<ObjectFlag>(OF_NO_BLOCK|OF_SEE_THROUGH|OF_SHOOT_THROUGH));
	}
	mSourceContainers=std::move(candidate);
}
// Mouse coordinates are converted to unzoomed world pixels exactly once: inside
// ScopedWorldInput (native world handlers and world rendering) they already are.
WorldViewZoom::Point TerrainSystem::SourceContainerPoint(int x,int y) const {
	return gameView->IsWorldInputActive() ? WorldViewZoom::Point{double(x),double(y)} : gameView->ToWorldScreen(x,y);
}
const SourceContainerRegion* TerrainSystem::HitSourceContainer(int x,int y,objHndl &handle) const {
	handle=objHndl::null;
	if (!gameView || !gameSystems || gameSystems->GetMap().IsClearingMap()) return nullptr;
	auto p=SourceContainerPoint(x,y);
	int ox,oy;GetTerrainOrigin(ox,oy);
	auto region=mSourceContainers.Hit(maps.GetCurrentMapId(),int(std::floor(p.x-ox)),int(std::floor(p.y-oy)));
	if (!region) return nullptr;
	auto found=objSystem->FindObjectByIdStr(region->objectId);
	if (!found || !IsSourceContainerProxy(found) || objects.IsUntargetable(found)
		|| (objects.GetFlags(found)&OF_DESTROYED)) return nullptr;
	if (!(gameSystems->GetMapFogging().IsPosExplored(objects.GetLocationFull(found))&1)) return nullptr;
	handle=found;
	return region;
}
uint64_t TerrainSystem::PickSourceContainer(int x,int y) const {
	objHndl handle;
	return HitSourceContainer(x,y,handle) ? handle.handle : 0;
}
void TerrainSystem::RenderSourceContainers() {
	// Rendering runs inside ScopedWorldInput: the mouse position is already in
	// world pixels, so the hover test and the drawn outline use the same point.
	auto mouse=mouseFuncs.GetPos();objHndl handle;
	auto region=HitSourceContainer(mouse.x,mouse.y,handle);
	if (!region) return;
	int ox,oy;GetTerrainOrigin(ox,oy);
	std::vector<gfx::Line2d> lines;
	for (size_t i=0;i<region->outline.size();++i) {
		const auto &a=region->outline[i], &b=region->outline[(i+1)%region->outline.size()];
		lines.emplace_back(XMFLOAT2(float(ox+a.x),float(oy+a.y)),XMFLOAT2(float(ox+b.x),float(oy+b.y)),XMCOLOR(0.2f,0.65f,1.0f,0.95f));
	}
	mShapeRenderer.DrawLines(lines);
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
	return mOriginalDoors.NavigationTiles(maps.GetCurrentMapId(), mDoorLineExclusion);
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
	CancelOriginalDoorUse();
	auto from = objects.GetLocationFull(actor).ToInches2D();
	mDoorUseQueue.Begin(mapId, found->first.second, actorHandle, mDoorUseRevision,
		timeGetTime(), GetOriginalDoorOpen(id), found->second, {from.x, from.y}, HasOriginalTravel() ? 60000 : 15000);
	AdvanceTime(timeGetTime()); // near use is immediate, but still requires a clear line
	if (mDoorUseQueue.Pending()) {
		auto target = mDoorUseQueue.Pending()->target;
		// The source point may itself be covered by the closed leaf. Ask the
		// native near-tile pathfinder for reachable floor within four feet,
		// then keep the independent five-foot distance and wall-ray check.
		auto dest = LocAndOffsets::FromInches(float(target.x), float(target.y));
		auto &anim = gameSystems->GetAnim();
		auto moving = HasOriginalTravel() ? anim.PushRunNearTile(actor, dest, 4) : anim.PushMoveToTile(actor, dest);
		if (!moving) {
			CancelOriginalDoorUse(); throw std::invalid_argument("cannot start movement to original door approach point");
		}
	}
}

void TerrainSystem::DispatchMapReady() {
	// 0x103072D4: temple.dll "loading a savegame" flag (see gamelib_load.cpp).
	if (gameSystems->GetMap().IsClearingMap() || !gameSystems->GetMap().IsMapOpen()
		|| temple::GetRef<int>(0x103072D4)) return;
	auto mapId=maps.GetCurrentMapId();
	// Main menu (5000) and character creation run with an empty party. Do not call
	// party.GetLeader() there: it dereferences group member 0 without a null check.
	if (mapId==5000 || party.GroupPCsLen()==0) return;
	auto first=party.GroupListGetMemberN(0);
	if (!first || !gameSystems->GetObj().IsValidHandle(first)) return;
	mMapReadyPending=false;
	logger->info("Map ready script: map={}",mapId);
	auto args=Py_BuildValue("(i)",mapId);
	auto result=pythonObjIntegration.ExecuteScript("templeplus.savehook","map_ready",args);
	Py_XDECREF(result);Py_DECREF(args);
}

void TerrainSystem::AdvanceTime(uint32_t time) {
	if (mMapReadyPending) DispatchMapReady();
	UpdateSceneFocus();
	AdvanceTravel(time);
	if (!mDoorUseQueue.Pending()) return;
	try {
		auto request = *mDoorUseQueue.Pending();
		if (gameSystems->GetMap().IsClearingMap() || maps.GetCurrentMapId() != request.mapId
			|| request.revision != mDoorUseRevision) { CancelOriginalDoorUse(); return; }
		objHndl actor{request.actor};
		auto valid = gameSystems->GetObj().IsValidHandle(actor) && party.IsInParty(actor)
			&& !critterSys.IsDeadOrUnconscious(actor)
			&& (!mContextRequest || SameDoorSelection(mContextRequest->selection,ContextSelection()));
		auto reachable = valid && !combatSys.isCombatActive()
			&& DoorUseReachable(request.actor, request.target, request.reach, request.id);
		auto action = mDoorUseQueue.Poll(maps.GetCurrentMapId(), mDoorUseRevision, time, valid,
			combatSys.isCombatActive(), GetOriginalDoorOpen(request.id), reachable);
		if (action == OriginalDoorUseQueue::Action::Apply) {
			// Finish movement before opening. Continuing toward the approach point
			// could otherwise cross the independent travel band after this click.
			StopDoorMovement(request.actor);
			if (mContextRequest) {
				auto context=*mContextRequest;mContextRequest.reset();ApplyDoorContext(context,request.actor);
			} else SetOriginalDoorOpen(request.id, request.open);
			logger->info("Original door approach applied: map={} id={} open={}", request.mapId, request.id, request.open);
		} else if (action == OriginalDoorUseQueue::Action::Cancelled) {
			mContextRequest.reset();
			logger->info("Original door approach cancelled: map={} id={}", request.mapId, request.id);
		}
	} catch (const std::exception &e) {
		CancelOriginalDoorUse(); logger->info("Original door approach refused: {}", e.what());
		auto actor = party.GetConsciousPartyLeader();
		if (actor) floatSys.floatMesLine(actor, 1, FloatLineColor::Yellow, e.what());
	}
}

bool TerrainSystem::DoorUseReachable(uint64_t handle, OriginalDoorUsePoint target, double reach, const std::string &excluded) {
	objHndl actor{handle};
	auto from = objects.GetLocationFull(actor);
	auto p = from.ToInches2D();
	if (OriginalDoorUseDistance({p.x,p.y}, target) > reach) return false;
	// A door must not block using its own handle. Keep all static walls and
	// every OTHER door in the ray; restore the exclusion even on an exception.
	struct Restore { std::string &value; std::string saved; ~Restore() { value=saved; } } restore{mDoorLineExclusion,mDoorLineExclusion};
	mDoorLineExclusion = excluded;
	RaycastPacket ray;
	ray.flags = static_cast<RaycastFlags>(HasToBeCleared | HasSourceObj | ExcludeItemObjects);
	ray.origin=from; ray.targetLoc=LocAndOffsets::FromInches(float(target.x),float(target.y));
	ray.sourceObj=actor; ray.radius=0; ray.Raycast();
	for (int i=0;i<ray.resultCount;++i) if (ray.results[i].flags & BlockerSubtile) return false;
	return true;
}

void TerrainSystem::StopDoorMovement(uint64_t handle) {
	objHndl actor{handle}; AnimSlotId moving{}; auto &anim=gameSystems->GetAnim();
	if ((anim.DoesObjHaveGoal(actor,ag_move_to_tile,&moving) || anim.DoesObjHaveGoal(actor,ag_run_to_tile,&moving)
		|| anim.DoesObjHaveGoal(actor,ag_move_near_tile,&moving) || anim.DoesObjHaveGoal(actor,ag_run_near_tile,&moving))
		&& !anim.Interrupt(actor,AnimGoalPriority::AGP_3,false))
		throw std::invalid_argument("cannot finish movement before door interaction");
}

void TerrainSystem::BindOriginalTravel(OriginalTravelMarker marker) {
	ValidateOriginalTravel(marker);
	if (!mOriginalDoors.HasNavigation(marker.gateMap,marker.gateId))
		throw std::invalid_argument("travel marker requires a registered navigation gate");
	if (mTravelMarkers.size() >= 128 && !mTravelMarkers.count({marker.mapId,marker.id}))
		throw std::invalid_argument("too many travel markers");
	mTravelMarkers[{marker.mapId,marker.id}]=std::move(marker);
	CancelOriginalDoorUse(); ++mDoorUseRevision;
}
void TerrainSystem::ClearOriginalTravel() {
	mTravelMarkers.clear(); mHoveredTravel.clear(); CancelOriginalDoorUse(); ++mDoorUseRevision;
}
bool TerrainSystem::HasOriginalTravel() const {
	for (const auto &entry:mTravelMarkers) if (entry.first.first==maps.GetCurrentMapId()) return true;
	return false;
}
std::string TerrainSystem::HitTestOriginalTravel(int x,int y) const {
	int ox,oy; GetTerrainOrigin(ox,oy); std::string hit;
	for (const auto &entry:mTravelMarkers) {
		if (entry.first.first!=maps.GetCurrentMapId() || !OriginalTravelHit(entry.second,x-ox,y-oy,gameView->GetWorldZoom())) continue;
		if (!hit.empty()) return {}; hit=entry.first.second;
	}
	return hit;
}
void TerrainSystem::RequestOriginalTravel(const std::string &id,uint64_t handle) {
	CancelOriginalDoorUse();
	auto entry=mTravelMarkers.find({maps.GetCurrentMapId(),OriginalDoorState::NormalizeId(id)});
	if (entry==mTravelMarkers.end()) throw std::invalid_argument("travel marker is not bound");
	objHndl actor{handle};
	if (!gameSystems->GetObj().IsValidHandle(actor) || !party.IsInParty(actor)
		|| critterSys.IsDeadOrUnconscious(actor) || combatSys.isCombatActive())
		throw std::invalid_argument("travel requires a conscious party actor outside combat");
	const auto &m=entry->second;
	if (mSceneRoutes.count(m.id)) {
		mTravelSelection=ContextSelection();
		if (mTravelSelection.empty() || mTravelSelection.size()!=party.CurrentlySelectedNum())
			throw std::invalid_argument("select conscious characters for travel");
		for (auto member:mTravelSelection) if (SceneAreaForActor(member)!=mSceneRoutes.at(m.id).source)
			throw std::invalid_argument("select characters in the same area");
	}
	if (!CanTravelThroughOriginalDoor(m.gateMap,m.gateId)) {
		floatSys.floatMesLine(actor,1,FloatLineColor::Yellow,"Open the door first"); return;
	}
	auto from=objects.GetLocationFull(actor).ToInches2D();
	mTravelQueue.Begin(m.mapId,m.id,handle,mDoorUseRevision,timeGetTime(),false,m.approach,{from.x,from.y},60000);
	logger->info("Original travel click: map={} route={}",m.mapId,m.id);
	AdvanceTravel(timeGetTime());
	if (mTravelQueue.Pending()) {
		auto target=mTravelQueue.Pending()->target;
		for (auto member:mTravelSelection) if (member!=handle)
			gameSystems->GetAnim().PushRunNearTile(objHndl{member},LocAndOffsets::FromInches(float(target.x),float(target.y)),6);
		if (!gameSystems->GetAnim().PushRunNearTile(actor,LocAndOffsets::FromInches(float(target.x),float(target.y)),4)) {
			CancelOriginalDoorUse(); throw std::invalid_argument("cannot approach travel marker");
		}
	}
}
std::optional<OriginalTravelReady::Intent> TerrainSystem::PeekOriginalTravel() {
	if (mTravelReady.Peek()) {
		auto intent=*mTravelReady.Peek(); objHndl actor{intent.actor};
		auto entry=mTravelMarkers.find({intent.mapId,intent.id});
		bool valid=!gameSystems->GetMap().IsClearingMap() && !combatSys.isCombatActive()
			&& gameSystems->GetObj().IsValidHandle(actor) && party.IsInParty(actor) && !critterSys.IsDeadOrUnconscious(actor)
			&& entry!=mTravelMarkers.end() && CanTravelThroughOriginalDoor(entry->second.gateMap,entry->second.gateId)
			&& (!mSceneRoutes.count(intent.id) || SameDoorSelection(mTravelSelection,ContextSelection()));
		mTravelReady.Validate(timeGetTime(),maps.GetCurrentMapId(),mDoorUseRevision,valid);
	}
	return mTravelReady.Peek();
}
bool TerrainSystem::AckOriginalTravel(uint32_t serial) {
	PeekOriginalTravel(); return mTravelReady.Ack(serial);
}
void TerrainSystem::AdvanceTravel(uint32_t time) {
	PeekOriginalTravel();
	if (!mTravelQueue.Pending()) return;
	try {
		auto request=*mTravelQueue.Pending();
		auto entry=mTravelMarkers.find({request.mapId,request.id});
		if (entry==mTravelMarkers.end() || gameSystems->GetMap().IsClearingMap()) { CancelOriginalDoorUse(); return; }
		const auto &m=entry->second; objHndl actor{request.actor};
		auto valid=gameSystems->GetObj().IsValidHandle(actor) && party.IsInParty(actor)
			&& !critterSys.IsDeadOrUnconscious(actor) && CanTravelThroughOriginalDoor(m.gateMap,m.gateId)
			&& (!mSceneRoutes.count(m.id) || SameDoorSelection(mTravelSelection,ContextSelection()));
		auto reachable=valid && request.mapId==maps.GetCurrentMapId() && !combatSys.isCombatActive()
			&& DoorUseReachable(request.actor,request.target,request.reach,m.gateMap==request.mapId ? m.gateId : "");
		if (reachable && mSceneRoutes.count(m.id)) {
			auto lead=objects.GetLocationFull(actor).ToInches2D();
			for (auto member:mTravelSelection) {
				auto p=objects.GetLocationFull(objHndl{member}).ToInches2D();
				if (SceneAreaForActor(member)!=mSceneRoutes.at(m.id).source || OriginalDoorUseDistance({lead.x,lead.y},{p.x,p.y})>360) reachable=false;
			}
		}
		auto action=mTravelQueue.Poll(maps.GetCurrentMapId(),mDoorUseRevision,time,valid,combatSys.isCombatActive(),false,reachable);
		if (action==OriginalDoorUseQueue::Action::Apply) {
			StopDoorMovement(request.actor);
			mTravelReady.Publish(time,request.mapId,request.id,mDoorUseRevision,request.actor);
			logger->info("Original travel ready: map={} route={}",request.mapId,request.id);
		} else if (action==OriginalDoorUseQueue::Action::Cancelled) {
			logger->info("Original travel cancelled: map={} route={}",request.mapId,request.id);
		}
	} catch (const std::exception &e) { CancelOriginalDoorUse(); logger->info("Original travel refused: {}",e.what()); }
}

void TerrainSystem::RenderOriginalTravel() {
	if (!HasOriginalTravel() || combatSys.isCombatActive()) return;
	if (HasDoorContext()) {RenderDoorContext();return;}
	int ox,oy; GetTerrainOrigin(ox,oy); auto zoom=gameView->GetWorldZoom();
	auto viewport=gameView->GetWorldViewport();
	auto triangle=[&](OriginalDoorUsePoint a,OriginalDoorUsePoint b,OriginalDoorUsePoint c,XMCOLOR color) {
		gfx::Vertex2d vertices[4]{}; OriginalDoorUsePoint points[4]={a,b,c,c};
		for (int i=0;i<4;++i) {
			vertices[i].pos=XMFLOAT4(float(points[i].x+ox),float(points[i].y+oy),0,1);
			vertices[i].diffuse=color;
		}
		mShapeRenderer.DrawRectangle(vertices, static_cast<gfx::Texture*>(nullptr));
	};
	for (const auto &entry:mTravelMarkers) {
		if (entry.first.first!=maps.GetCurrentMapId()) continue;
		const auto &m=entry.second;
		auto r=29/zoom;
		if (m.x+ox+r<viewport.left || m.x+ox-r>viewport.right || m.y+oy+r<viewport.top || m.y+oy-r>viewport.bottom) continue;
		auto enabled=CanTravelThroughOriginalDoor(m.gateMap,m.gateId);
		auto pending=(mTravelQueue.Pending() && mTravelQueue.Pending()->id==m.id) || (mTravelReady.Peek() && mTravelReady.Peek()->id==m.id);
		XMCOLOR gold=enabled ? XMCOLOR(0.90f,0.70f,0.30f,1.0f) : XMCOLOR(0.48f,0.46f,0.40f,0.85f);
		if (pending || mHoveredTravel==m.id) gold=XMCOLOR(0.55f,0.85f,1.0f,1.0f);
		std::vector<gfx::Line2d> ring;
		for (int i=0;i<32;++i) {
			double a=i*6.283185307179586/32,b=(i+1)*6.283185307179586/32;
			OriginalDoorUsePoint p{m.x+r*cos(a),m.y+r*sin(a)},q{m.x+r*cos(b),m.y+r*sin(b)};
			triangle({m.x,m.y},p,q,XMCOLOR(0.07f,0.05f,0.03f,0.78f));
			ring.emplace_back(XMFLOAT2(float(p.x+ox),float(p.y+oy)),XMFLOAT2(float(q.x+ox),float(q.y+oy)),gold);
		}
		mShapeRenderer.DrawLines(ring);
		auto arrow=OriginalTravelArrow(m,zoom);
		triangle(arrow[0],arrow[1],arrow[5],gold); triangle(arrow[0],arrow[5],arrow[6],gold);
		triangle(arrow[2],arrow[3],arrow[4],gold);
		if (!enabled) {
			gfx::Line2d slash[1]={{XMFLOAT2(float(m.x+ox-18/zoom),float(m.y+oy+18/zoom)),XMFLOAT2(float(m.x+ox+18/zoom),float(m.y+oy-18/zoom)),gold}};
			mShapeRenderer.DrawLines(slash);
		}
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
	ClearDoorContext();
	mOriginalDoors.ClearMap(maps.GetCurrentMapId());
	ClearOriginalTravel();
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
	return mOriginalDoors.HasBindings(maps.GetCurrentMapId()) || HasOriginalTravel();
}
std::string TerrainSystem::HitTestOriginalDoor(int screenX, int screenY) const {
	if (!HasOriginalDoorInput()) return {};
	int x, y; GetTerrainOrigin(x, y);
	return HasOriginalTravel() ? mOriginalDoors.HitTestStable(maps.GetCurrentMapId(), screenX-x, screenY-y)
		: mOriginalDoors.HitTest(maps.GetCurrentMapId(), screenX-x, screenY-y);
}

void TerrainSystem::Reset() {
	ClearSourceContainers();
	ClearDoorContext();
	mOriginalDoors.Clear();
	mDoorUse.clear(); ClearOriginalTravel(); CancelOriginalDoorUse(); ++mDoorUseRevision;
	mHoveredOriginalDoor.clear();
}

void TerrainSystem::UnloadModule() {
	ClearSourceContainers();
	ClearDoorContext();
	mOriginalDoors.Clear();
	mDoorUse.clear(); ClearOriginalTravel(); CancelOriginalDoorUse(); ++mDoorUseRevision;
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
		if (HasOriginalTravel()) {
			auto entry = mOriginalDoors.Outlines().find({mapId, mHoveredOriginalDoor});
			if (entry != mOriginalDoors.Outlines().end()) polygon = &entry->second.closed;
		}
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
	for (const auto &entry:mSceneAreas) {
		const auto &area=entry.second;
		if (area.sourceGround && area.nativeMap==maps.GetCurrentMapId() && x*256<area.x+area.width
			&& (x+1)*256>area.x && y*256<area.y+area.height && (y+1)*256>area.y) {
			mapArtId=area.sourceGround;x-=area.shiftX/256;y-=area.shiftY/256;break;
		}
	}


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
