#include "../../TemplePlus/graphics/originaldoorstate.h"
#include <cassert>
#include <iostream>

static OriginalDoorPatch MakePatch(int mapId = 5136, const std::string &id = "DOOR2616") {
	return OriginalDoorPatch{ mapId, id, 6872, 7602, 128, 192,
		{ "art/bg2toee/doors/2616/day_closed.png", "art/bg2toee/doors/2616/day_open.png",
		  "art/bg2toee/doors/2616/night_closed.png", "art/bg2toee/doors/2616/night_open.png" }, false };
}

template<typename F> static void Reject(F fn) {
	bool rejected = false;
	try { fn(); } catch (const std::invalid_argument &) { rejected = true; }
	assert(rejected);
}

int main() {
	OriginalDoorState state;
	state.Register(MakePatch());
	assert(!state.Find(5136, "door2616")->open);
	state.SetOpen(5136, "DOOR2616", true);
	state.Register(MakePatch()); // registration must not reset a revisited door
	assert(state.Find(5136, "DOOR2616")->open);
	state.Register(MakePatch(5190));
	assert(!state.Find(5190, "DOOR2616")->open);
	state.Register(MakePatch(5136, "OTHER_DOOR"));
	assert(!state.Find(5136, "OTHER_DOOR")->open);
	assert(OriginalDoorState::TextureIndex(false, false) == 0);
	assert(OriginalDoorState::TextureIndex(true, false) == 1);
	assert(OriginalDoorState::TextureIndex(false, true) == 2);
	assert(OriginalDoorState::TextureIndex(true, true) == 3);
	Reject([&]() { state.SetOpen(5136, "MISSING", true); });
	for (auto path : { "C:/game/file.png", "art/bg2toee/doors/../bad.png", "art/bg2toee/doors/file.jpg" }) {
		auto patch = MakePatch();
		patch.textures[0] = path;
		Reject([&]() { state.Register(patch); });
		assert(state.Find(5136, "DOOR2616")->open); // failed update is non-mutating
	}
	auto patch = MakePatch();
	patch.width = 0;
	Reject([&]() { state.Register(patch); });
	patch = MakePatch();
	patch.x = 66 * 256;
	Reject([&]() { state.Register(patch); });
	Reject([&]() { OriginalDoorState::NormalizeId("../door"); });
	state.ClearMap(5136);
	assert(!state.Find(5136, "DOOR2616"));
	assert(state.Find(5190, "DOOR2616"));
	state.Clear();
	assert(state.Patches().empty());
	std::cout << "Original door registry tests passed\n";
}
