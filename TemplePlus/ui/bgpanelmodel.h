#pragma once
#include <cstdint>
#include <string>
#include <vector>

/*
	Model of the bg2toee panel: a modal full-screen window whose content the
	mod's Python builds (world map, camp, inn). Pure data and geometry, shared
	by the engine and standalone tests.
*/
namespace bgpanel {

enum class Kind { Rect = 0, Image = 1, Portrait = 2, Text = 3, Button = 4 };

struct Element {
	Kind kind = Kind::Rect;
	std::string id;
	int x = 0, y = 0, w = 0, h = 0;
	uint32_t color = 0xFFFFFFFF;   // fill, image tint or text colour (ARGB)
	uint32_t border = 0;           // outline colour, 0 = none
	std::string path;              // image texture
	std::string text;              // cp1251 text or button label
	int align = 0;                 // text: 0 left, 1 centre
	uint64_t obj = 0;              // portrait: critter handle
	std::string tip;               // tooltip, cp1251
	bool click = false;            // reports clicks
	bool enabled = true;           // button
};

inline bool Contains(const Element &e, int x, int y) {
	return x >= e.x && y >= e.y && x < e.x + e.w && y < e.y + e.h;
}

// Topmost (last drawn) element under the point that reports clicks; -1 if none.
inline int HitTest(const std::vector<Element> &elements, int x, int y) {
	for (int i = int(elements.size()) - 1; i >= 0; --i) {
		const auto &e = elements[i];
		if (e.click && e.enabled && Contains(e, x, y)) return i;
	}
	return -1;
}

// Topmost element under the point with a tooltip; -1 if none.
inline int TooltipAt(const std::vector<Element> &elements, int x, int y) {
	for (int i = int(elements.size()) - 1; i >= 0; --i) {
		const auto &e = elements[i];
		if (Contains(e, x, y)) return e.tip.empty() ? -1 : i;
	}
	return -1;
}

struct Box { int x, y, w, h; };

// Tooltip box beside the cursor, kept inside the canvas.
inline Box TooltipBox(int mouseX, int mouseY, int w, int h, int canvasW, int canvasH) {
	Box b{ mouseX + 18, mouseY + 18, w, h };
	if (b.x + b.w > canvasW) b.x = mouseX - 10 - b.w;
	if (b.y + b.h > canvasH) b.y = mouseY - 10 - b.h;
	if (b.x < 0) b.x = 0;
	if (b.y < 0) b.y = 0;
	return b;
}

inline bool ValidKind(int kind) { return kind >= 0 && kind <= 4; }
constexpr size_t MaxElements = 600;

}
