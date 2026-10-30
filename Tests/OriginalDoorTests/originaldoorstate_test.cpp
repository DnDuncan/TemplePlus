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

static void TestOutlines() {
	OriginalDoorState state;
	state.Register(MakePatch());
	OriginalDoorOutline outline{ {{10,10},{40,10},{40,40},{10,40}}, {{50,10},{55,10},{55,40},{50,40}} };
	assert(state.HitTest(5136, 20, 20).empty()); // registration does not enable input
	Reject([&]() { state.Bind(5136, "MISSING", outline); });
	state.Bind(5136, "door2616", outline);
	assert(state.HitTest(5136, 20, 20) == "DOOR2616");
	assert(state.HitTest(5136, 10, 25) == "DOOR2616"); // boundary included
	assert(state.HitTest(5136, 9, 25).empty());
	assert(state.HitTest(5190, 20, 20).empty());
	state.SetOpen(5136, "DOOR2616", true);
	assert(state.HitTest(5136, 20, 20).empty());
	assert(state.HitTest(5136, 52, 25) == "DOOR2616");
	state.Register(MakePatch());
	assert(state.HitTest(5136, 52, 25) == "DOOR2616"); // revisit keeps state/binding
	auto bad = outline;
	bad.closed = {{1,1},{2,2},{3,3}};
	Reject([&]() { state.Bind(5136, "DOOR2616", bad); });
	assert(state.HitTest(5136, 52, 25) == "DOOR2616"); // rejected update is non-mutating
	bad = outline; bad.open[0].x = -1;
	Reject([&]() { state.Bind(5136, "DOOR2616", bad); });
	bad = outline; bad.open.resize(257);
	Reject([&]() { state.Bind(5136, "DOOR2616", bad); });
	state.Register(MakePatch(5136, "SECOND"));
	state.Bind(5136, "SECOND", outline);
	assert(state.HitTest(5136, 52, 25) == "DOOR2616");
	state.SetOpen(5136, "SECOND", true);
	assert(state.HitTest(5136, 52, 25).empty()); // overlap is ambiguous
	state.Unbind(5136, "SECOND");
	assert(state.HitTest(5136, 52, 25) == "DOOR2616");
	state.ClearMap(5136);
	assert(state.HitTest(5136, 52, 25).empty());
	state.Register(MakePatch());
	assert(!state.Outline(5136, "DOOR2616"));
	state.Bind(5136, "DOOR2616", outline);
	state.Clear();
	assert(state.HitTest(5136, 20, 20).empty());
	OriginalDoorPolygon concave{ {0,0},{10,0},{10,4},{4,4},{4,10},{0,10} };
	ValidateDoorPolygon(concave);
	assert(DoorPolygonContains(concave, 2, 8));
	assert(!DoorPolygonContains(concave, 8, 8));
	std::reverse(concave.begin(), concave.end());
	assert(DoorPolygonContains(concave, 2, 8));
}

static void TestMousePress() {
	OriginalDoorMousePress press;
	using Event = OriginalDoorMousePress::Event;
	assert(!press.Process(5136, "DOOR", Event::Up, 10, 10).consumed);
	assert(!press.Process(5136, "", Event::Down, 10, 10).consumed);
	assert(press.Process(5136, "DOOR", Event::Up, 10, 10).toggle.empty());
	assert(press.Process(5136, "DOOR", Event::Down, 10, 10).consumed);
	assert(press.Process(5136, "DOOR", Event::Move, 12, 12).toggle.empty());
	assert(press.Process(5136, "DOOR", Event::Up, 12, 12).toggle == "DOOR");
	assert(press.Process(5136, "DOOR", Event::Up, 12, 12).toggle.empty()); // exactly once
	press.Process(5136, "DOOR", Event::Down, 10, 10);
	assert(press.Process(5136, "", Event::Up, 10, 10).consumed); // release off outline never walks
	press.Process(5136, "DOOR", Event::Down, 10, 10);
	assert(press.Process(5136, "OTHER", Event::Up, 10, 10).toggle.empty());
	press.Process(5136, "DOOR", Event::Down, 10, 10);
	press.Process(5136, "DOOR", Event::Move, 30, 10);
	assert(press.Process(5136, "DOOR", Event::Up, 10, 10).toggle.empty()); // drag back still cancels
	press.Process(5136, "DOOR", Event::Down, 10, 10);
	assert(press.Process(5190, "DOOR", Event::Up, 10, 10).toggle.empty());
	press.Process(5136, "DOOR", Event::Down, 10, 10);
	press.Cancel(); // UI/modal/combat/map reset
	assert(!press.Process(5136, "DOOR", Event::Up, 10, 10).consumed);
}

int main() {
	TestOutlines();
	TestMousePress();
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
