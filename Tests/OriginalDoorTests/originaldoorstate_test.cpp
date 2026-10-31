#include "../../TemplePlus/graphics/originaldoorstate.h"
#include "../../TemplePlus/graphics/originaldoorsave.h"
#include "../../TemplePlus/graphics/originaldooruse.h"
#include <limits>
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
	assert(!state.HasBindings(5136));
	Reject([&]() { state.Bind(5136, "MISSING", outline); });
	state.Bind(5136, "door2616", outline);
	assert(state.HasBindings(5136) && !state.HasBindings(5190));
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
	assert(!state.HasBindings(5136));
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

static void TestNavigation() {
	OriginalDoorState state;
	state.Register(MakePatch());
	state.Register(MakePatch(5190));
	DoorNavigationGeometry geometry{{{10, 20, 0x200}, {11, 20, 0x400}}, {{12, 20, 0x800}}};
	state.BindNavigation(5136, "door2616", geometry);
	assert(!state.CanTravel(5136, "DOOR2616"));
	assert(!state.CanTravel(5190, "DOOR2616"));
	assert(state.NavigationTiles(5136).size() == 2);
	state.SetOpen(5136, "DOOR2616", true);
	assert(state.CanTravel(5136, "DOOR2616"));
	assert(state.NavigationTiles(5136).size() == 1);
	assert(state.NavigationTiles(5136)[0].x == 12);
	assert(state.NavigationTiles(5190).empty());
	state.Register(MakePatch(5136, "OTHER"));
	state.BindNavigation(5136, "OTHER", {{{12, 20, 0x200}}, {}});
	assert(state.NavigationTiles(5136)[0].mask == 0xa00); // independent doors OR, never erase each other
	state.UnbindNavigation(5136, "OTHER");
	assert(state.NavigationTiles(5136)[0].mask == 0x800);
	for (auto bad : {DoorNavigationGeometry{{{10, 20, 1}}, {}}, DoorNavigationGeometry{{{-1,20,0x200}}, {}},
		DoorNavigationGeometry{{{10,20,0x200},{10,20,0x400}}, {}}, DoorNavigationGeometry{{}, {}}}) {
		Reject([&]() { state.BindNavigation(5136, "DOOR2616", bad); });
		assert(state.CanTravel(5136, "DOOR2616"));
		assert(state.NavigationTiles(5136)[0].mask == 0x800);
	}
	uint32_t flags = 0x8000200;
	try {
		TemporaryDoorTileMasks<uint32_t> outer;
		outer.Add(flags, 0x400);
		assert(flags == 0x8000600);
		{ TemporaryDoorTileMasks<uint32_t> inner; inner.Add(flags, 0x800); assert(flags == 0x8000e00); }
		assert(flags == 0x8000600);
		outer.Add(flags, 0x1000); // repeated pointer also restores in reverse order
		throw std::runtime_error("synthetic native query failure");
	} catch (const std::runtime_error&) {}
	assert(flags == 0x8000200); // unrelated base blocker and cover are intact
	constexpr double subtile = 28.2842712474619 / 3;
	std::vector<DoorNavigationTile> cells{{10,20,0x200}};
	auto x = 30.5 * subtile, y = 60.5 * subtile;
	assert(DoorNavigationBlocksSegment(cells, x-30, y, x+30, y, 0));
	assert(DoorNavigationBlocksSegment(cells, x, y, x, y, 10)); // actor under the leaf
	assert(!DoorNavigationBlocksSegment(cells, x-30, y+40, x+30, y+40, 10));
	state.ClearMap(5136);
	assert(!state.HasNavigation(5136, "DOOR2616"));
	assert(!state.CanTravel(5136, "DOOR2616"));
	assert(state.NavigationTiles(5136).empty());
}

static void TestSave() {
	OriginalDoorState state;
	state.Register(MakePatch()); state.Register(MakePatch(5190));
	state.Bind(5136, "DOOR2616", {{{10,10},{40,10},{40,40}}, {{50,10},{55,10},{55,40}}});
	state.BindNavigation(5136, "DOOR2616", {{{10,20,0x200}}, {{11,20,0x400}}});
	state.SetOpen(5136, "DOOR2616", true);
	auto bytes = originaldoorsave::Encode(state, "{\"transition\":true}");
	auto restored = originaldoorsave::Decode(bytes);
	assert(restored.metadata == "{\"transition\":true}");
	assert(restored.doors.Patches().size() == 2);
	assert(restored.doors.CanTravel(5136, "door2616"));
	assert(!restored.doors.CanTravel(5190, "door2616"));
	assert(restored.doors.Outline(5136, "door2616")->front().x == 50);
	assert(restored.doors.NavigationTiles(5136).front().x == 11);
	restored.doors.SetOpen(5136, "door2616", false);
	assert(restored.doors.NavigationTiles(5136).front().x == 10);
	assert(originaldoorsave::Encode(originaldoorsave::Decode(bytes).doors, restored.metadata) == bytes);
	for (size_t i = 0; i < bytes.size(); ++i) {
		std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + i);
		Reject([&]() { originaldoorsave::Decode(truncated); });
	}
	auto bad = bytes; bad.push_back(0); Reject([&]() { originaldoorsave::Decode(bad); });
	bad = bytes; bad[4] = 2; Reject([&]() { originaldoorsave::Decode(bad); });
	bad = bytes; bad[8] = 255; bad[9] = 255; Reject([&]() { originaldoorsave::Decode(bad); });
	Reject([&]() { originaldoorsave::Encode(state, std::string(4097, 'x')); });
	OriginalDoorState empty;
	assert(originaldoorsave::Decode(originaldoorsave::Encode(empty, "")).doors.Patches().empty());
}

static void TestUse() {
	OriginalDoorUseGeometry geometry{{{100,200}, {300,400}}, 60};
	OriginalDoorUseQueue queue;
	using Action = OriginalDoorUseQueue::Action;
	assert(queue.Poll(5136, 0, 0, true, false, false, true) == Action::Idle);
	queue.Begin(5136, "DOOR2616", 42, 7, 100, false, geometry, {310,410});
	assert(queue.Pending()->target.x == 300); // source candidate nearest the actor
	assert(queue.Pending()->open);
	assert(queue.Poll(5136, 7, 200, true, false, false, false) == Action::Waiting);
	assert(queue.Poll(5136, 7, 300, true, false, false, true) == Action::Apply);
	assert(queue.Poll(5136, 7, 301, true, false, true, true) == Action::Idle); // consumed once
	for (int reason = 0; reason < 6; ++reason) {
		queue.Begin(5136, "DOOR2616", 42, 7, 100, false, geometry, {100,200});
		assert(queue.Poll(reason == 0 ? 5190 : 5136, reason == 1 ? 8 : 7,
			reason == 2 ? 15101 : 200, reason != 3, reason == 4, reason == 5, true) == Action::Cancelled);
	}
	queue.Begin(5136, "DOOR2616", 42, 7, 0xfffffff0, true, geometry, {100,200});
	assert(!queue.Pending()->open);
	assert(queue.Poll(5136, 7, 20, true, false, true, false) == Action::Waiting); // clock wrap
	queue.Cancel(); assert(!queue.Pending());
	assert(OriginalDoorUseDistance({0,0}, {36,48}) == 60);
	for (double reach : {0.0, -1.0, 60.01, std::numeric_limits<double>::infinity()}) {
		auto bad = geometry; bad.reach = reach; Reject([&]() { ValidateOriginalDoorUse(bad); });
	}
	auto bad = geometry; bad.points.clear(); Reject([&]() { ValidateOriginalDoorUse(bad); });
	bad = geometry; bad.points[0].x = std::numeric_limits<double>::quiet_NaN();
	Reject([&]() { ValidateOriginalDoorUse(bad); });
	bad = geometry; bad.points[0].y = 960 * 28.2842712474619;
	Reject([&]() { ValidateOriginalDoorUse(bad); });
}

int main() {
	TestUse();
	TestSave();
	TestNavigation();
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
