#pragma once

#include "../gamesystems/gamesystem.h"
#include <temple/dll.h>
#include "originaldoorstate.h"
#include "originaldooruse.h"
#include "originaltravel.h"
#include "originaldoorcontext.h"
#include "sourcecontainers.h"
#include "../worldviewzoom.h"
#include <infrastructure/location.h>

struct TigRect;

namespace gfx {
	class RenderingDevice;
	class ShapeRenderer2d;
}

class TerrainSystem : public GameSystem, public ModuleAwareGameSystem, public ResetAwareGameSystem, public TimeAwareGameSystem {
public:
	static constexpr auto Name = "Terrain";

	/**
	 * Both width and height of a single tile in pixels.
	 */
	static constexpr auto TileSize = 256;

	/**
	 * Max. dimensions of a map in tiles.
	 */
	static constexpr auto MapWidthTiles = 66;
	static constexpr auto MapHeightTiles = 71;

	/**
	* Max. dimensions of a map in pixels.
	*/
	static constexpr auto MapWidthPixels = MapWidthTiles * TileSize;
	static constexpr auto MapHeightPixels = MapHeightTiles * TileSize;

	// Time in MS that we fade between the day/night versions upon transitioning
	static constexpr auto TransitionTime = 2000;

	// Offset for the map art ids for nighttime ids
	static constexpr auto NightArtIdOffset = 1000;

	TerrainSystem(gfx::RenderingDevice &device,
		gfx::ShapeRenderer2d &shapeRenderer);
	~TerrainSystem() override;

	void Render();

	void RenderTile(int x, int y, const TigRect& destRect);
	
	const std::string &GetName(void) const override;

	void Load(int groundArtId);
	
	void LoadModule() override;
	void UnloadModule() override;
	void Reset() override;
	void RegisterOriginalDoor(OriginalDoorPatch patch);
	void SetOriginalDoorOpen(const std::string &id, bool open);
	bool GetOriginalDoorOpen(const std::string &id) const;
	void ClearOriginalDoors();
	void BindOriginalDoor(const std::string &id, OriginalDoorOutline outline);
	void UnbindOriginalDoor(const std::string &id);
	bool IsOriginalDoorBound(const std::string &id) const;
	bool HasOriginalDoorInput() const;
	std::string HitTestOriginalDoor(int screenX, int screenY) const;
	void GetTerrainOrigin(int &x, int &y) const;
	void SetHoveredOriginalDoor(const std::string &id) { mHoveredOriginalDoor = id; }
	void BindOriginalDoorNavigation(const std::string &id, DoorNavigationGeometry geometry);
	void UnbindOriginalDoorNavigation(const std::string &id, int mapId = 0);
	bool HasOriginalDoorNavigation(const std::string &id) const;
	bool CanTravelThroughOriginalDoor(int mapId, const std::string &id) const;
	const std::vector<DoorNavigationTile> &GetOriginalDoorBlockingTiles() const;
	void SaveOriginalDoorSession(const std::string &metadata) const;
	std::string LoadOriginalDoorSession();
	bool GetOriginalDoorOpenOnMap(int mapId, const std::string &id) const;
	void BindOriginalDoorUse(int mapId, const std::string &id, OriginalDoorUseGeometry geometry);
	void UnbindOriginalDoorUse(int mapId, const std::string &id);
	bool HasOriginalDoorUse(int mapId, const std::string &id) const;
	void RequestOriginalDoorUse(const std::string &id, uint64_t actor);
	void CancelOriginalDoorUse() { mDoorUseQueue.Cancel(); mTravelQueue.Cancel(); mTravelReady.Cancel(); mContextRequest.reset(); mTravelSelection.clear(); }
	void BindOriginalTravel(OriginalTravelMarker marker);
	void ClearOriginalTravel();
	bool HasOriginalTravel() const;
	std::string HitTestOriginalTravel(int x, int y) const;
	void SetHoveredOriginalTravel(const std::string &id) { mHoveredTravel = id; }
	void RequestOriginalTravel(const std::string &id, uint64_t actor);
	std::optional<OriginalTravelReady::Intent> PeekOriginalTravel();
	bool AckOriginalTravel(uint32_t serial);
	void RenderOriginalTravel();
	void BindDoorContext(int mapId,const std::string &id,DoorContextDefinition definition);
	DoorSecurity GetDoorSecurity(int mapId,const std::string &id) const;
	void BindContextOutline(int mapId,const std::string &route,OriginalDoorPolygon outline);
	void ClearDoorContext();
	bool HasDoorContext() const;
	std::string UpdateDoorContextHover(int x,int y);
	void ClearDoorContextHover() {mContextHover.Clear();mHoveredContextAction=-1;}
	void RequestDoorContextAction(const std::string &route,int action);
	void RequestDoorContextDefault(const std::string &route);
	bool HasDoorContextHover() const {return !mContextHover.Target().empty();}
	bool HoveredDoorLocked() const;
	int SearchDoorTraps(objHndl actor,LocAndOffsets center,float radius,int total,bool reveal);
	void BindSceneArea(OriginalSceneArea area);
	void BindSceneRoute(const std::string &route,OriginalSceneRoute definition);
	int SceneAreaForActor(uint64_t actor) const;
	int FocusedSceneArea() const {return mFocusedScene;}
	bool TravelSelectedInScene(uint32_t serial);
	void AdvanceTime(uint32_t time) override;
	void BindSourceContainer(SourceContainerRegion region, const OriginalDoorUseGeometry *use=nullptr,
        const std::array<std::string,2> *icons=nullptr);
	void ClearSourceDoors() {mSourceDoors.clear();}
	void ClearSourceContainers();
	// Baldur's Gate doors backed by an invisible native portal: native lock,
	// trap, Search / Open Lock / Disable Device and opening; the source door
	// art and navigation follow the portal's open state.
	void BindSourceDoor(int mapId,const std::string &doorId,const std::string &objectId);
	bool HasSourceDoor(int mapId,const std::string &doorId) const;
	std::string SourceDoorFor(objHndl handle) const;
	void SyncSourceDoor(objHndl handle);
	bool SourceDoorReachable(objHndl actor,objHndl portal);
	bool PrepareSourceDoorToggle(objHndl portal);
	uint64_t PickSourceContainer(int x,int y) const;
	const SourceContainerRegion* HitSourceContainer(int x,int y,objHndl &handle) const;
	WorldViewZoom::Point SourceContainerPoint(int x,int y) const;
	// Ask the mod's Python layer to (re)bind map-specific objects once the
	// current map and party are fully restored (teleport or savegame load).
	void RequestMapReadyScript() {mMapReadyPending=true;}

	TerrainSystem(TerrainSystem&) = delete;
	TerrainSystem(TerrainSystem&&) = delete;
	TerrainSystem& operator=(TerrainSystem&) = delete;
	TerrainSystem& operator=(TerrainSystem&&) = delete;
	
	void UpdateDayNight();
	// bg2toee Search: a ground template (radius in inches) shown until untilMs (timeGetTime).
	void AddSearchTemplate(LocAndOffsets center, float radiusInches, uint32_t untilMs);

private:
	struct SearchTemplate { int mapId; float x, z, radius; uint32_t until; };
	std::vector<SearchTemplate> mSearchTemplates;
	void RenderSearchTemplates();
	gfx::RenderingDevice& mDevice;
	gfx::ShapeRenderer2d& mShapeRenderer;

	int mMapArtId = 0;
	
	// Tracking for the day/night transition
	bool mIsNightTime = false;
	bool mIsTransitioning = false;
	uint32_t mTransitionStart = 0;
	float mTransitionProgress = 0;

	// Terrain colors are managed by the daylight system
	float &mTerrainTintRed = temple::GetRef<float>(0x11E69574);
	float &mTerrainTintGreen = temple::GetRef<float>(0x11E69570);
	float &mTerrainTintBlue = temple::GetRef<float>(0x11E69564);

	std::unordered_map<int, std::string> mTerrainDirs;
	OriginalDoorState mOriginalDoors;
	SourceContainerRegions mSourceContainers;
	std::map<OriginalDoorState::Key,std::string> mContextContainers;
	bool ContextIsOpen(int mapId,const std::string &id) const;
	void SetContextSecurity(int mapId,const std::string &id,const DoorSecurity &security);
	void SpringContainerTrap(objHndl container,objHndl actor);
	std::map<OriginalDoorState::Key,std::string> mSourceDoors; // door -> portal object id
	mutable SourceContainerRegion mSourceDoorHit;
	void RenderSourceContainers();
	std::map<OriginalDoorState::Key, OriginalDoorUseGeometry> mDoorUse;
	OriginalDoorUseQueue mDoorUseQueue, mTravelQueue;
	std::map<OriginalDoorState::Key, OriginalTravelMarker> mTravelMarkers;
	OriginalTravelReady mTravelReady;
	std::map<OriginalDoorState::Key,DoorContextDefinition> mDoorContexts;
	std::map<OriginalDoorState::Key,OriginalDoorPolygon> mContextOutlines;
	DoorContextHover mContextHover;
	int mHoveredContextAction=-1;
	struct ContextRequest { int gateMap;std::string gate,route;DoorContextAction action;std::vector<uint64_t> selection; };
	std::optional<ContextRequest> mContextRequest;
	std::vector<uint64_t> mTravelSelection;
	std::map<int,OriginalSceneArea> mSceneAreas;
	std::map<std::string,OriginalSceneRoute> mSceneRoutes;
	int mFocusedScene=0;
	std::optional<std::array<int64_t,4>> mSavedSceneLimits;
	std::vector<uint64_t> ContextSelection() const;
	std::vector<OriginalDoorUsePoint> ContextLayout(const OriginalTravelMarker &marker,size_t count) const;
	void RenderDoorContext();
	void ApplyDoorContext(const ContextRequest &request,uint64_t actor);
	bool ContextTravel(const OriginalTravelMarker &marker) const;
	std::vector<DoorContextAction> ContextActions(const OriginalTravelMarker &marker) const;
	void SpringDoorTrap(int mapId,const std::string &id,objHndl actor);
	bool mMapReadyPending=false,mAppearanceReady=false;
	void DispatchMapReady();
	void UpdateSceneFocus();
	int SceneAreaAt(OriginalDoorUsePoint point) const;
	std::string mHoveredTravel, mDoorLineExclusion;
	bool DoorUseReachable(uint64_t actor, OriginalDoorUsePoint target, double reach, const std::string &excluded);
	void StopDoorMovement(uint64_t actor);
	void AdvanceTravel(uint32_t time);
	uint64_t mDoorUseRevision = 0;
	std::string mHoveredOriginalDoor;
	void RenderOriginalDoors(int terrainOriginX, int terrainOriginY);
	void CheckNewDoorBlockers(const OriginalDoorState &candidate) const;
	void ValidateOriginalDoorTextures(const OriginalDoorPatch &patch) const;

	std::string GetTileTexture(int mapArtId, int x, int y);

};

// Raycast hooks can run during system construction/shutdown.
extern TerrainSystem *originalDoorTerrain;
