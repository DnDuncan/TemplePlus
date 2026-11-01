#pragma once
#include "originaldoorstate.h"
#include <set>

// Separate archive member: never append to the native data.sav system stream.
// Bounded, little-endian and independent of struct packing / engine pointers.
namespace originaldoorsave {
constexpr size_t MaxBytes = 16 * 1024 * 1024;
constexpr uint32_t MaxDoors = 1024;
struct Session { OriginalDoorState doors; std::string metadata; };

class Writer {
public:
	std::vector<uint8_t> bytes;
	void U32(uint32_t value) { for (int i = 0; i < 4; ++i) bytes.push_back(uint8_t(value >> (i * 8))); }
	void String(const std::string &value, size_t limit) {
		if (value.size() > limit) throw std::invalid_argument("original door save string exceeds limit");
		U32(static_cast<uint32_t>(value.size())); bytes.insert(bytes.end(), value.begin(), value.end());
	}
	void Polygon(const OriginalDoorPolygon &points) {
		ValidateDoorPolygon(points); U32(static_cast<uint32_t>(points.size()));
		for (const auto &p : points) { U32(p.x); U32(p.y); }
	}
	void Tiles(const std::vector<DoorNavigationTile> &tiles) {
		ValidateDoorNavigation(tiles); U32(static_cast<uint32_t>(tiles.size()));
		for (const auto &t : tiles) { U32(t.x); U32(t.y); U32(t.mask); }
	}
};

class Reader {
public:
	explicit Reader(const std::vector<uint8_t> &bytes) : mBytes(bytes) {
		if (bytes.size() > MaxBytes) throw std::invalid_argument("original door save exceeds size limit");
	}
	uint32_t U32() {
		if (mPos > mBytes.size() || mBytes.size() - mPos < 4) throw std::invalid_argument("truncated original door save");
		uint32_t value = 0; for (int i = 0; i < 4; ++i) value |= uint32_t(mBytes[mPos++]) << (i * 8);
		return value;
	}
	uint32_t Count(uint32_t limit) {
		auto value = U32(); if (value > limit) throw std::invalid_argument("original door save count exceeds limit");
		return value;
	}
	int Integer(uint32_t limit) { return static_cast<int>(Count(limit)); }
	std::string String(uint32_t limit) {
		auto size = Count(limit);
		if (mPos > mBytes.size() || mBytes.size() - mPos < size) throw std::invalid_argument("truncated original door save string");
		std::string result(mBytes.begin() + mPos, mBytes.begin() + mPos + size); mPos += size;
		if (result.find('\0') != std::string::npos) throw std::invalid_argument("NUL in original door save string");
		return result;
	}
	OriginalDoorPolygon Polygon() {
		auto count = Count(256); OriginalDoorPolygon result;
		for (uint32_t i = 0; i < count; ++i) { auto x = Integer(66 * 256 - 1); auto y = Integer(71 * 256 - 1); result.push_back({x, y}); }
		ValidateDoorPolygon(result); return result;
	}
	std::vector<DoorNavigationTile> Tiles() {
		auto count = Count(256); std::vector<DoorNavigationTile> result;
		for (uint32_t i = 0; i < count; ++i) { auto x = Integer(959); auto y = Integer(959); auto mask = U32(); result.push_back({x, y, mask}); }
		ValidateDoorNavigation(result); return result;
	}
	bool Done() const { return mPos == mBytes.size(); }
private:
	const std::vector<uint8_t> &mBytes; size_t mPos = 0;
};

inline std::vector<uint8_t> Encode(const OriginalDoorState &state, const std::string &metadata) {
	if (state.Patches().size() > MaxDoors) throw std::invalid_argument("too many original doors to save");
	Writer w; w.U32(0x52444742); w.U32(1); w.String(metadata, 4096); w.U32(static_cast<uint32_t>(state.Patches().size()));
	for (const auto &entry : state.Patches()) {
		const auto &p = entry.second;
		w.U32(p.mapId); w.String(p.id, 32); w.U32(p.x); w.U32(p.y); w.U32(p.width); w.U32(p.height);
		for (const auto &texture : p.textures) w.String(texture, 240);
		w.U32(p.open ? 1 : 0);
		auto outline = state.Outlines().find(entry.first); auto nav = state.NavigationBindings().find(entry.first);
		w.U32((outline != state.Outlines().end() ? 1 : 0) | (nav != state.NavigationBindings().end() ? 2 : 0));
		if (outline != state.Outlines().end()) { w.Polygon(outline->second.closed); w.Polygon(outline->second.open); }
		if (nav != state.NavigationBindings().end()) { w.Tiles(nav->second.closed); w.Tiles(nav->second.open); }
	}
	if (w.bytes.size() > MaxBytes) throw std::invalid_argument("original door save exceeds size limit");
	return std::move(w.bytes);
}

inline Session Decode(const std::vector<uint8_t> &bytes) {
	Reader r(bytes);
	if (r.U32() != 0x52444742 || r.U32() != 1) throw std::invalid_argument("unsupported original door save format");
	Session result; result.metadata = r.String(4096); auto count = r.Count(MaxDoors);
	std::set<OriginalDoorState::Key> seen;
	for (uint32_t i = 0; i < count; ++i) {
		OriginalDoorPatch p; p.mapId = r.Integer(0x7fffffff); p.id = r.String(32);
		p.x = r.Integer(66 * 256); p.y = r.Integer(71 * 256); p.width = r.Integer(1024); p.height = r.Integer(1024);
		for (auto &texture : p.textures) texture = r.String(240);
		p.open = r.Count(1) != 0; auto flags = r.Count(3);
		OriginalDoorState::Validate(p);
		if (!seen.emplace(p.mapId, p.id).second) throw std::invalid_argument("duplicate original door in save");
		result.doors.Register(p);
		if (flags & 1) { auto closed = r.Polygon(); auto open = r.Polygon(); result.doors.Bind(p.mapId, p.id, {closed, open}); }
		if (flags & 2) { auto closed = r.Tiles(); auto open = r.Tiles(); result.doors.BindNavigation(p.mapId, p.id, {closed, open}); }
	}
	if (!r.Done()) throw std::invalid_argument("trailing bytes in original door save");
	return result;
}
}
