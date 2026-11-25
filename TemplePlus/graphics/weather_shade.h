#pragma once

#include <algorithm>
#include <cstdint>

class WeatherShade {
public:
	static constexpr uint32_t FadeMs = 1200;

	int Opacity(uint32_t now) const {
		auto elapsed = (std::min)(now - mStarted, FadeMs);
		return (mFrom * int(FadeMs - elapsed) + mTarget * int(elapsed)) / int(FadeMs);
	}

	void Set(int alpha, uint32_t now) {
		alpha = (std::clamp)(alpha, 0, 200);
		if (alpha == mTarget) return;
		mFrom = Opacity(now);
		mTarget = alpha;
		mStarted = now;
	}

private:
	int mFrom = 0;
	int mTarget = 0;
	uint32_t mStarted = 0;
};
