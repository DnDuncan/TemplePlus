#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Pure policy shared by the runtime and the headless regression tests.
namespace bgskillrules {
constexpr float SearchRadius = 20.0f * 12.0f;
constexpr uint32_t SearchDuration = 6000;
class SearchAttempt {
public:
	void Start(uint32_t now) {started=now;active=true;resolved=false;}
	void Cancel() {active=false;}
	bool Poll(uint32_t now) {
		if (!active || resolved) return false;
		auto elapsed=uint32_t(now-started);
		if (elapsed>=SearchDuration) {active=false;return false;}
		if (elapsed<SearchDuration/2) return false;
		resolved=true;return true;
	}
private:
	uint32_t started=0;
	bool active=false,resolved=false;
};
constexpr uint32_t ToolStamp = 0x42475400; // BGT; low byte is the saved streak

inline bool InSearchArea(float cx, float cz, float x, float z) {
	const auto dx = x - cx, dz = z - cz;
	return dx * dx + dz * dz <= SearchRadius * SearchRadius;
}
inline int DisableFailureDelta(int natural, int delta) {
	return natural == 1 ? (std::min)(delta, -10) : (std::max)(delta, -3);
}
inline int ToolStreak(uint32_t saved) {
	return (saved & 0xFFFFFF00u) == ToolStamp ? int(saved & 0xFFu) : 0;
}
inline uint32_t RecordToolRoll(uint32_t saved, bool criticalFailure) {
	return ToolStamp | (criticalFailure ? (std::min)(255, ToolStreak(saved) + 1) : 0);
}
inline bool ToolBreaks(uint32_t saved, int enhancement) {
	return ToolStreak(saved) >= 2 + (std::max)(0, enhancement);
}
}
