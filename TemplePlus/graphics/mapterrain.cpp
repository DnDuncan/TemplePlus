
#include "stdafx.h"
#include "mapterrain.h"
#include "maps.h"
#include <tig/tig.h>
#include <graphics/device.h>
#include <graphics/shaperenderer2d.h>
#include <infrastructure/mesparser.h>
#include <config/config.h>
#include <util/fixes.h>
#include <gamesystems/gamesystems.h>
#include <gamesystems/timeevents.h>

TerrainSystem::TerrainSystem(gfx::RenderingDevice& device, gfx::ShapeRenderer2d& shapeRenderer)
	: mDevice(device),mShapeRenderer(shapeRenderer) {

	// Previously initialized in ground_init
	mTerrainTintRed = 1.0f;
	mTerrainTintGreen = 1.0f;
	mTerrainTintBlue = 1.0f;

}

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

	auto viewportWidth = config.renderWidth;
	auto viewportHeight = config.renderHeight;

	// The terrain is centered on the center tile of the map
	// This is 480,480 for all vanilla maps, but theoretically it 
	// depends on the map size specified in the map's map.prp file
	auto mapCenter = maps.GetMapCenterTile();

	// The center of the map in pixels relative to the current screen viewport
	int terrainOriginX, terrainOriginY;
	locSys.ToTranslation(mapCenter.locx, mapCenter.locy, terrainOriginX, terrainOriginY);

	// Since the origin is still pointing at the map center, shift it left/up by half
	// the terrains overall size. The Map is slightly off-center for *some* unknown
	// reason. This may be a relic from Arkanum
	terrainOriginX -= MapWidthPixels / 2 - 40;
	terrainOriginY -= MapHeightPixels / 2 - 14;

	auto startX = - terrainOriginX / TileSize;
	auto startY = - terrainOriginY / TileSize;

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
			if (destRect.x + TileSize >= viewportWidth) {
				break;
			}
		}

		// The next column would be out of view
		if (destRect.y + TileSize >= viewportHeight) {
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
	mOriginalDoors.Register(std::move(patch));
}

void TerrainSystem::SetOriginalDoorOpen(const std::string &id, bool open) {
	mOriginalDoors.SetOpen(maps.GetCurrentMapId(), id, open);
}

bool TerrainSystem::GetOriginalDoorOpen(const std::string &id) const {
	auto patch = mOriginalDoors.Find(maps.GetCurrentMapId(), id);
	if (!patch) throw std::invalid_argument("original door is not registered on this map");
	return patch->open;
}

void TerrainSystem::ClearOriginalDoors() {
	mOriginalDoors.ClearMap(maps.GetCurrentMapId());
}

void TerrainSystem::Reset() {
	mOriginalDoors.Clear();
}

void TerrainSystem::UnloadModule() {
	mOriginalDoors.Clear();
	mTerrainDirs.clear();
	mMapArtId = 0;
}

void TerrainSystem::RenderOriginalDoors(int terrainOriginX, int terrainOriginY) {
	auto mapId = maps.GetCurrentMapId();
	for (const auto &entry : mOriginalDoors.Patches()) {
		const auto &patch = entry.second;
		if (patch.mapId != mapId) continue;
		auto x = terrainOriginX + patch.x;
		auto y = terrainOriginY + patch.y;
		if (x + patch.width <= 0 || y + patch.height <= 0 ||
			x >= config.renderWidth || y >= config.renderHeight) continue;
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
