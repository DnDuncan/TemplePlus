#pragma once
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct OriginalDoorUsePoint { double x, y; }; // world inches, never screen pixels
struct OriginalDoorUseGeometry { std::vector<OriginalDoorUsePoint> points; double reach = 60; };

inline void ValidateOriginalDoorUse(const OriginalDoorUseGeometry &geometry) {
	if (geometry.points.empty() || geometry.points.size() > 2 || !std::isfinite(geometry.reach)
		|| geometry.reach <= 0 || geometry.reach > 60)
		throw std::invalid_argument("door use requires 1..2 approach points and reach within five feet");
	for (const auto &p : geometry.points) if (!std::isfinite(p.x) || !std::isfinite(p.y)
		|| p.x < 0 || p.y < 0 || p.x >= 960 * 28.2842712474619 || p.y >= 960 * 28.2842712474619)
		throw std::invalid_argument("door approach point is outside the world map");
}

inline double OriginalDoorUseDistance(OriginalDoorUsePoint a, OriginalDoorUsePoint b) {
	return std::hypot(a.x - b.x, a.y - b.y);
}

class OriginalDoorUseQueue {
public:
	enum class Action { Idle, Waiting, Apply, Cancelled };
	struct Request {
		int mapId; std::string id; uint64_t actor, revision; uint32_t started;
		bool open; OriginalDoorUsePoint target; double reach;
	};
	void Begin(int mapId, const std::string &id, uint64_t actor, uint64_t revision,
		uint32_t now, bool currentOpen, const OriginalDoorUseGeometry &geometry, OriginalDoorUsePoint from) {
		ValidateOriginalDoorUse(geometry);
		if (mapId <= 0 || id.empty() || id.size() > 32 || !actor || !std::isfinite(from.x) || !std::isfinite(from.y))
			throw std::invalid_argument("invalid original door use request");
		auto target = geometry.points.front();
		for (const auto &point : geometry.points)
			if (OriginalDoorUseDistance(from, point) < OriginalDoorUseDistance(from, target)) target = point;
		mRequest = Request{mapId, id, actor, revision, now, !currentOpen, target, geometry.reach};
	}
	const std::optional<Request> &Pending() const { return mRequest; }
	void Cancel() { mRequest.reset(); }
	Action Poll(int mapId, uint64_t revision, uint32_t now, bool actorCanUse,
		bool inCombat, bool currentOpen, bool reachable) {
		if (!mRequest) return Action::Idle;
		if (mRequest->mapId != mapId || mRequest->revision != revision || !actorCanUse
			|| inCombat || uint32_t(now - mRequest->started) > 15000 || currentOpen == mRequest->open) {
			Cancel(); return Action::Cancelled;
		}
		if (!reachable) return Action::Waiting;
		Cancel(); return Action::Apply; // consume BEFORE mutating the door; never toggle twice
	}
private:
	std::optional<Request> mRequest;
};
