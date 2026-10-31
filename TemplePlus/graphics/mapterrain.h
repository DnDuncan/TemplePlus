#pragma once

#include "../gamesystems/gamesystem.h"
#include <temple/dll.h>
#include "originaldoorstate.h"

struct TigRect;

namespace gfx {
	class RenderingDevice;
	class ShapeRenderer2d;
}

class TerrainSystem : public GameSystem, public ModuleAwareGameSystem, public ResetAwareGameSystem {
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

	TerrainSystem(TerrainSystem&) = delete;
	TerrainSystem(TerrainSystem&&) = delete;
	TerrainSystem& operator=(TerrainSystem&) = delete;
	TerrainSystem& operator=(TerrainSystem&&) = delete;
	
	void UpdateDayNight();
private:
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
	std::string mHoveredOriginalDoor;
	void RenderOriginalDoors(int terrainOriginX, int terrainOriginY);
	void CheckNewDoorBlockers(const OriginalDoorState &candidate) const;
	void ValidateOriginalDoorTextures(const OriginalDoorPatch &patch) const;

	std::string GetTileTexture(int mapArtId, int x, int y);

};

// Raycast hooks can run during system construction/shutdown.
extern TerrainSystem *originalDoorTerrain;
