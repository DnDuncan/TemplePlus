#pragma once
#include "originaldoorstate.h"
#include "originaldooruse.h"
#include <array>

// Marker coordinates are terrain-canvas pixels; approach coordinates are inches.
// The shape and hit target keep their display size when the world is zoomed.
struct OriginalTravelMarker {
	int mapId, gateMap; std::string id, gateId;
	double x, y, dx, dy;
	OriginalDoorUseGeometry approach;
};
inline void ValidateOriginalTravel(const OriginalTravelMarker &m) {
	if (m.mapId <= 0 || m.gateMap <= 0 || m.id != OriginalDoorState::NormalizeId(m.id)
		|| m.gateId != OriginalDoorState::NormalizeId(m.gateId)
		|| !std::isfinite(m.x) || !std::isfinite(m.y) || m.x < 0 || m.y < 0
		|| m.x >= 66*256 || m.y >= 71*256 || !std::isfinite(m.dx) || !std::isfinite(m.dy)
		|| !std::isfinite(std::hypot(m.dx,m.dy)) || std::hypot(m.dx, m.dy) < 0.001)
		throw std::invalid_argument("invalid original travel marker");
	ValidateOriginalDoorUse(m.approach);
}
inline bool OriginalTravelHit(const OriginalTravelMarker &m, double x, double y, double zoom) {
	return zoom > 0 && std::hypot(x-m.x, y-m.y)*zoom <= 30;
}
inline std::array<OriginalDoorUsePoint, 7> OriginalTravelArrow(const OriginalTravelMarker &m, double zoom) {
	if (!(zoom > 0) || !std::isfinite(zoom)) throw std::invalid_argument("invalid marker zoom");
	auto length = std::hypot(m.dx, m.dy);
	auto ux = m.dx/length/zoom, uy = m.dy/length/zoom;
	std::array<OriginalDoorUsePoint, 7> result{};
	const double shape[7][2] = {{-19,-5},{3,-5},{3,-13},{22,0},{3,13},{3,5},{-19,5}};
	for (size_t i=0; i<result.size(); ++i)
		result[i] = {m.x + ux*shape[i][0] - uy*shape[i][1], m.y + uy*shape[i][0] + ux*shape[i][1]};
	return result;
}

// One-shot handshake with Python. No queued or ready click is saved in a game.
class OriginalTravelReady {
public:
	struct Intent { uint32_t serial, started; int mapId; std::string id; uint64_t revision, actor; };
	void Publish(uint32_t now, int mapId, const std::string &id, uint64_t revision, uint64_t actor) {
		if (!++mSerial) ++mSerial;
		mIntent = Intent{mSerial, now, mapId, id, revision, actor};
	}
	const std::optional<Intent> &Peek() const { return mIntent; }
	void Validate(uint32_t now, int mapId, uint64_t revision, bool valid) {
		if (mIntent && (!valid || mIntent->mapId != mapId || mIntent->revision != revision
			|| uint32_t(now-mIntent->started) > 15000)) Cancel();
	}
	bool Ack(uint32_t serial) {
		if (!mIntent || mIntent->serial != serial) return false;
		Cancel(); return true;
	}
	void Cancel() { mIntent.reset(); }
private:
	uint32_t mSerial = 0;
	std::optional<Intent> mIntent;
};
