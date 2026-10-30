#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

// Presentation coordinates only; source area pixels and world locations stay fixed.
class WorldViewZoom {
public:
	struct Point { double x, y; };
	struct Bounds { double left, top, right, bottom; };
	double Scale() const { return std::pow(2.0, mStep / 6.0); }
	void Reset() { mStep = 0; mRemainder = 0; }
	void Set(double scale) {
		if (!std::isfinite(scale) || scale < 0.5 || scale > 2.0)
			throw std::invalid_argument("camera zoom must be between 0.5 and 2.0");
		mStep = static_cast<int>(std::round(6.0 * std::log2(scale))); mRemainder = 0;
	}
	bool Wheel(int delta) {
		auto total = static_cast<int64_t>(mRemainder) + delta;
		auto steps = total / 120; mRemainder = static_cast<int>(total % 120);
		auto next = (std::max)(int64_t(-6), (std::min)(int64_t(6), int64_t(mStep) + steps));
		auto changed = next != mStep; mStep = static_cast<int>(next); return changed;
	}
	Point ToWorld(double x, double y, double width, double height) const {
		auto z = Scale(); return { width / 2 + (x - width / 2) / z, height / 2 + (y - height / 2) / z };
	}
	Point ToDisplay(double x, double y, double width, double height) const {
		auto z = Scale(); return { width / 2 + (x - width / 2) * z, height / 2 + (y - height / 2) * z };
	}
	Bounds Visible(double width, double height) const {
		auto a = ToWorld(0, 0, width, height), b = ToWorld(width, height, width, height);
		return { a.x, a.y, b.x, b.y };
	}
private:
	int mStep = 0, mRemainder = 0;
};
