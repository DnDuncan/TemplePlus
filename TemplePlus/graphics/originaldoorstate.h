#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

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
	}

	void ClearMap(int mapId) {
		for (auto it = mPatches.begin(); it != mPatches.end();) {
			if (it->first.first == mapId) it = mPatches.erase(it);
			else ++it;
		}
	}

	void Clear() { mPatches.clear(); }
	using Key = std::pair<int, std::string>;
	using Entry = std::pair<const Key, OriginalDoorPatch>;
	const std::map<Key, OriginalDoorPatch> &Patches() const { return mPatches; }
	static std::size_t TextureIndex(bool open, bool night) { return (night ? 2u : 0u) + (open ? 1u : 0u); }

private:
	std::map<Key, OriginalDoorPatch> mPatches;
};
