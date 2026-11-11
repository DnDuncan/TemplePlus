#include "../../TemplePlus/graphics/originaldoorstate.h"
#include "../../TemplePlus/graphics/originaldoorsave.h"
#include "../../TemplePlus/graphics/originaldooruse.h"
#include "../../TemplePlus/graphics/originaltravel.h"
#include "../../TemplePlus/graphics/originaldoorcontext.h"
#include "../../TemplePlus/ui/bgpanelmodel.h"
#include "../../TemplePlus/graphics/sourcecontainers.h"
#include "../../TemplePlus/worldviewzoom.h"
#include "../../TemplePlus/ui/bgskillrules.h"
#include "../../TemplePlus/bgcheckstate.h"
#include "../../TemplePlus/bgappearance.h"
#include "../../TemplePlus/bginteriornavigation.h"
#include "../../TemplePlus/bgmovementrules.h"
#include "../../TemplePlus/bgconsumables.h"
#include "../../TemplePlus/ui/bgdicegeometry.h"
#include "../../TemplePlus/bgscenecombatpolicy.h"
#include "../../TemplePlus/bgactorcontext.h"
#include "../../TemplePlus/bgengagementrules.h"
#include "../../TemplePlus/bgencounters.h"
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
	auto largeMetadata=std::string(20*1024,'x');
    assert(originaldoorsave::Decode(originaldoorsave::Encode(state,largeMetadata)).metadata==largeMetadata);
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
	Reject([&]() { originaldoorsave::Encode(state, std::string(originaldoorsave::MaxMetadataBytes+1, 'x')); });
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
	queue.Begin(5136, "DOOR2616", 42, 7, 100, false, geometry, {100,200}, 60000);
	assert(queue.Poll(5136, 7, 20100, true, false, false, false) == Action::Waiting);
	assert(queue.Poll(5136, 7, 60101, true, false, false, true) == Action::Cancelled);
	Reject([&]() {queue.Begin(5136, "DOOR2616", 42, 7, 100, false, geometry, {100,200}, 60001);});
	OriginalDoorMousePress press;
	using Event = OriginalDoorMousePress::Event;
	assert(press.Process(5136, "DOOR2616", Event::Down, 0, 0, 12).consumed);
	assert(press.Process(5136, "DOOR2616", Event::Up, 9, 0, 12).toggle == "DOOR2616");
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

static void TestClickTravel() {
	OriginalTravelMarker m{5136,5136,"DOOR2616A","DOOR2616",7000,7800,-60,-40,{{{13000,11000}},60}};
	ValidateOriginalTravel(m);
	for (double zoom : {0.5,1.0,2.3}) {
		assert(OriginalTravelHit(m,m.x+29/zoom,m.y,zoom));
		assert(!OriginalTravelHit(m,m.x+31/zoom,m.y,zoom));
		auto arrow=OriginalTravelArrow(m,zoom);
		assert(std::abs(OriginalDoorUseDistance({m.x,m.y},arrow[3])*zoom-22)<0.00001);
		assert((arrow[3].x-m.x)*m.dx+(arrow[3].y-m.y)*m.dy>0);
	}
	for (int bad=0;bad<4;++bad) {
		auto invalid=m;
		if (bad==0) invalid.mapId=0;
		if (bad==1) invalid.x=-1;
		if (bad==2) invalid.dx=invalid.dy=0;
		if (bad==3) invalid.approach.reach=61;
		Reject([&]() {ValidateOriginalTravel(invalid);});
	}
	OriginalTravelReady ready;
	ready.Publish(100,5136,m.id,7,42); auto serial=ready.Peek()->serial;
	assert(!ready.Ack(serial+1) && ready.Peek());
	assert(ready.Ack(serial) && !ready.Peek() && !ready.Ack(serial));
	for (int reason=0;reason<4;++reason) {
		ready.Publish(100,5136,m.id,7,42);
		ready.Validate(reason==0 ? 15101:200,reason==1 ? 5190:5136,reason==2 ? 8:7,reason!=3);
		assert(!ready.Peek());
	}
	ready.Publish(0xfffffff0,5190,"DOOR2600",7,42); ready.Validate(20,5190,7,true); assert(ready.Peek());
	ready.Cancel(); assert(!ready.Peek());
	OriginalDoorState state; state.Register(MakePatch());
	state.Bind(5136,"DOOR2616",{{{10,10},{40,10},{40,40},{10,40}},{{50,10},{55,10},{55,40},{50,40}}});
	state.SetOpen(5136,"DOOR2616",true);
	assert(state.HitTest(5136,20,20).empty());
	assert(state.HitTestStable(5136,20,20)=="DOOR2616");
	assert(state.HitTestStable(5136,52,20)=="DOOR2616");
	assert(state.HitTestStable(5136,5,20)=="DOOR2616");
	assert(state.HitTestStable(5136,0,20).empty());
	state.BindNavigation(5136,"DOOR2616",{{{1,2,0x200}},{{1,2,0x400}}});
	state.Register(MakePatch(5136,"OTHER")); state.BindNavigation(5136,"OTHER",{{{1,2,0x800}}, {}});
	assert(state.NavigationTiles(5136)[0].mask==0xc00);
	assert(state.NavigationTiles(5136,"DOOR2616")[0].mask==0x800);
	assert(state.NavigationTiles(5136)[0].mask==0xc00); // scoped own-door ray cannot leak into movement
	state.Register(MakePatch(5136,"OVERLAP")); state.Bind(5136,"OVERLAP",{{{10,10},{40,10},{40,40},{10,40}},{{10,10},{40,10},{40,40},{10,40}}});
	assert(state.HitTestStable(5136,20,20).empty());
}

static void TestContextAndScenes() {
	using A=DoorContextAction;
	DoorSecurity security;assert((DoorContextActions(false,security)==std::vector<A>{A::Open}));
	security.locked=true;auto actions=DoorContextActions(false,security);assert((actions==std::vector<A>{A::Search,A::PickLock}));
	assert(DoorContextActions(true,security).back()==A::Travel);
	assert((DoorContextActions(true,security,false)==std::vector<A>{A::Close})); // no scene route, no travel icon
	security.trapped=true;assert((DoorContextActions(false,security)==std::vector<A>{A::Search,A::PickLock})); // unknown trap stays hidden
	security.found=true;assert((DoorContextActions(false,security)==std::vector<A>{A::Search,A::Disarm,A::PickLock}));
	assert((DoorContextActions(false,security,true,false)==std::vector<A>{A::Search,A::PickLock})); // legacy five icons
	security.locked=false;assert((DoorContextActions(false,security)==std::vector<A>{A::Open,A::Disarm}));
	auto sprung=DoorTrapSprung(security);assert(!sprung.trapped && !sprung.found && sprung.disarmDc==security.disarmDc);
	assert(!DoorDisarmSpringsTrap(-4) && DoorDisarmSpringsTrap(-5) && !DoorDisarmSpringsTrap(0));
	DoorSecurity badFound;badFound.found=true;Reject([&](){ValidateDoorSecurity(badFound);});
	DoorSecurity badDisarm;badDisarm.disarmDc=61;Reject([&](){ValidateDoorSecurity(badDisarm);});
	security=DoorSecurity{};
	assert(BestDoorSkillCandidate({{1,8,10,true},{2,15,40,true},{3,99,5,false}})==2); // wizard wins on bonus, not class
	assert(BestDoorSkillCandidate({{1,-5,10,true},{2,-2,40,true}})==2);
	assert(BestDoorSkillCandidate({{1,10,40,true},{2,10,10,true}})==2);
	assert(BestDoorSkillCandidate({{1,99,10,false}})==0);
	DoorContextHover hover;hover.Update("DOOR",100);assert(!hover.Visible(549));assert(hover.Visible(550));
	assert(hover.Visible(5000)); // stationary mouse still opens the delayed palette
	hover.Update("",5100);assert(hover.Visible(5300));hover.Update("DOOR",5301);assert(hover.Visible(5301));
	hover.Update("OTHER",5400);assert(!hover.Visible(5800));hover.Update("",5900);assert(!hover.Visible(6251));
	hover.Update("DOOR",0xfffffff0);assert(hover.Visible(500));hover.Clear();assert(!hover.Active(600));
	OriginalSceneArea outer{5136,5136,5848,7154,5120,3840,0,0,0},inner{5190,5136,7928,14098,960,704,901,0,5376};
	ValidateOriginalSceneArea(outer);ValidateOriginalSceneArea(inner);
	constexpr double unit=28.2842712474619;
	auto before=SceneCanvasPoint(483.5*unit,486.5*unit),after=SceneCanvasPoint((483.5+192)*unit,(486.5+192)*unit);
	assert(std::abs(after.x-before.x)<0.00001 && std::abs(after.y-before.y-5376)<0.00001);
	assert(SceneContains(inner,after.x,after.y) && !SceneContains(outer,after.x,after.y));
	assert(SameDoorSelection({1,2},{2,1}));assert(!SameDoorSelection({1,2},{1,3}));assert(!SameDoorSelection({},{}));
	auto bad=inner;bad.shiftY=5377;Reject([&](){ValidateOriginalSceneArea(bad);});
	security.searchDc=61;Reject([&](){ValidateDoorSecurity(security);});
	security.searchDc=10;security.found=true;Reject([&](){ValidateDoorSecurity(security);});
}

static void TestSelectedArrivalPlanning() {
	std::vector<SceneArrivalActor> actors{{10,25},{20,20},{30,25},{40,25},{50,25}};
	std::vector<SceneArrivalSpot> plan;
	assert(PlanSceneArrival(actors,{1000,1000},[](auto,auto){return true;},plan));
	assert(plan.size()==actors.size());
	for (size_t i=0;i<plan.size();++i) {
		assert(plan[i].id==actors[i].id); // same selected IDs; no unselected/copy IDs
		for (size_t j=0;j<i;++j) assert(OriginalDoorUseDistance(plan[i].point,plan[j].point)>=plan[i].radius+plan[j].radius);
	}
	auto before=plan;
	assert(!PlanSceneArrival(actors,{1000,1000},[](auto actor,auto){return actor.id!=30;},plan));
	assert(plan.size()==before.size() && plan[0].point.x==before[0].point.x); // no partial result when third actor cannot fit
	assert(!PlanSceneArrival(std::vector<SceneArrivalActor>{},{1000,1000},[](auto,auto){return true;},plan));
	assert(!PlanSceneArrival(std::vector<SceneArrivalActor>{{10,25},{10,25}},{1000,1000},[](auto,auto){return true;},plan));
	assert(PlanSceneArrival(actors,{1000,1000},[](auto,auto p){return p.x>=1000 && p.y>=1000;},plan));
	for (const auto &spot:plan) assert(spot.point.x>=1000 && spot.point.y>=1000);
}

static void TestSourceContainerRegions() {
	SourceContainerRegions regions;
	OriginalDoorPolygon poly={{100,100},{140,100},{140,110},{110,110},{110,140},{100,140}};
	regions.Bind({5136,"BARREL","GUID_A",poly});
	assert(regions.Hit(5136,105,130)->objectId=="GUID_A");
	assert(!regions.Hit(5136,120,120)); // concave notch is not the bounding rectangle
	assert(regions.Hit(5136,100,125)); // exact outline edge is clickable
	assert(!regions.Hit(5190,105,130));
	regions.Bind({5136,"OVERLAP","GUID_B",poly});
	assert(!regions.Hit(5136,105,130)); // never pick arbitrary inventory
	regions.Clear();assert(!regions.Hit(5136,105,130));
	Reject([&](){regions.Bind({5136,"BAD","",poly});});
	Reject([&](){regions.Bind({5136,"BAD","GUID",{{0,0},{1,1},{2,2}}});});
	for (double scale:{0.5,1.0,2.3}) {
		WorldViewZoom zoom;zoom.Set(scale);
		auto screen=zoom.ToDisplay(105,130,1440,960);
		auto logical=zoom.ToWorld(screen.x,screen.y,1440,960);
		regions.Bind({5136,"BARREL","GUID_A",poly});
		assert(regions.Hit(5136,int(std::round(logical.x)),int(std::round(logical.y))));
	}
}

static void TestManualCheckLifecycle() {
    using namespace bgchecks;
    SlotBudget slots;
    assert(slots.Reserve(1,130,2,1));
    assert(!slots.Reserve(1,130,2,1)); // two buffs cannot spend the same final slot
    assert(slots.Reserve(2,130,2,1)); // another caster has a separate pool
    assert(slots.Reserve(1,130,3,1));
    assert(slots.Reserve(1,131,2,1));
    assert(!slots.Reserve(1,130,4,0));
    CheckState c;
    assert(!c.Commit()); // preparation cannot mutate game state
    assert(!c.Roll(100,0,3,10));
    assert(c.Roll(100,12,3,15));
    assert(!c.Roll(101,20,100,1)); // double click cannot reroll
    assert(!c.Cancel()); // rolled failure cannot be cancelled into a free retry
    assert(!c.Commit());
    c.Advance(1899);assert(c.phase==Phase::Rolling);
    c.Advance(1900);assert(c.phase==Phase::Result && c.Success(true));
    int paid=0,effects=0;
    if(c.Commit()){++paid;++effects;}
    if(c.Commit()){++paid;++effects;}
    assert(paid==1 && effects==1);
    CheckState cancelled;assert(cancelled.Cancel());assert(!cancelled.Roll(0,20,0,10));assert(!cancelled.Commit());
    CheckState one;one.Roll(0,1,40,10);assert(!one.Success(true) && one.Success(false));
    CheckState twenty;twenty.Roll(0,20,0,30);assert(!twenty.Success(true)); // no 5e automatic success
    CheckState wrap;wrap.duration=6000;wrap.Roll(0xFFFFFF00u,8,2,19);
    wrap.Advance(0xFFFFFF00u+5999u);assert(wrap.phase==Phase::Rolling);
    wrap.Advance(0xFFFFFF00u+6000u);assert(wrap.phase==Phase::Result);
}

static void TestReviewedShankScene() {
    OriginalSceneArea a{5192,5136,2968,8818,640,512,903,-5120,0};
    ValidateOriginalSceneArea(a);assert(IsReviewedCandlekeepScene(a));
    auto bad=a;bad.id=5193;assert(!IsReviewedCandlekeepScene(bad));
    bad=a;++bad.x;assert(!IsReviewedCandlekeepScene(bad));
    bad=a;bad.sourceGround=904;assert(!IsReviewedCandlekeepScene(bad));
    bad=a;bad.shiftX=0;assert(!IsReviewedCandlekeepScene(bad));
    bad=a;bad.nativeMap=5192;assert(!IsReviewedCandlekeepScene(bad));
}
static void TestSceneNightGround() {
    assert(SceneGroundArt(909,true,true)==1909);
    assert(SceneGroundArt(909,false,true)==909);
    assert(SceneGroundArt(903,true,false)==903); // no night registry entry: day art
    assert(LightningFlashMs(-5)==0 && LightningFlashMs(120)==120 && LightningFlashMs(5000)==1000);
    assert(LightningFlashActive(100,50) && !LightningFlashActive(100,100) && !LightningFlashActive(100,150));
    assert(LightningFlashActive(5,0xFFFFFFF0u)); // across the timer wrap
}
static void TestBgPanelModel() {
    using namespace bgpanel;
    std::vector<Element> e(3);
    e[0].x=0;e[0].y=0;e[0].w=100;e[0].h=100;e[0].click=true;e[0].id="back";
    e[1].x=10;e[1].y=10;e[1].w=20;e[1].h=20;e[1].click=true;e[1].id="area";e[1].tip="tip";
    e[2].x=40;e[2].y=40;e[2].w=20;e[2].h=20;e[2].click=true;e[2].enabled=false;
    assert(HitTest(e,15,15)==1 && HitTest(e,5,5)==0 && HitTest(e,45,45)==0 && HitTest(e,150,5)==-1);
    assert(TooltipAt(e,15,15)==1 && TooltipAt(e,5,5)==-1);
    auto b=TooltipBox(990,700,100,50,1024,768);
    assert(b.x==880 && b.y==640);
    b=TooltipBox(10,10,100,50,1024,768);
    assert(b.x==28 && b.y==28);
    assert(ValidKind(4) && !ValidKind(5) && !ValidKind(-1));
}
int main() {
    TestReviewedShankScene();
    TestBgPanelModel();
    TestSceneNightGround();
    for (const auto &area : {OriginalSceneArea{5194,5136,2968,12402,640,512,905,-5120,3584},
                            OriginalSceneArea{5195,5136,13208,8882,640,384,906,5120,0},
                            OriginalSceneArea{5196,5136,13144,10610,768,512,907,5120,1792}}) {
        ValidateOriginalSceneArea(area);assert(IsReviewedCandlekeepScene(area));
        auto bad=area;++bad.sourceGround;assert(!IsReviewedCandlekeepScene(bad));
    }
    {
        struct Route { int flags; int nodes[3]; };
        struct Result: Route { int occupied; int pause; };
        Result source,slot;
        source.flags=1;source.nodes[0]=4;source.nodes[1]=7;source.nodes[2]=9;
        source.occupied=99;source.pause=888;
        slot.flags=0;slot.occupied=1;slot.pause=33;
        BgCopyPreparedRoute<Route>(slot,source);
        assert(slot.flags==1 && slot.nodes[0]==4 && slot.nodes[1]==7 && slot.nodes[2]==9);
        assert(slot.occupied==1 && slot.pause==33); // live slot bookkeeping is preserved
    }
    // L-shaped narrow corridor. Every emitted segment stays on proven floor;
    // skipping the bend would cross the blocked interior of the L.
    {
        const std::vector<std::pair<int,int>> astar={{0,0},{1,0},{2,0},{2,1},{2,2}};
        std::vector<std::pair<int,int>> route;
        assert(BgEmitInteriorSubtileRoute(4,200,[&](int i,bool last) {
            route.push_back(last?astar.back():astar[i]);
        }));
        auto previous=astar.front();
        for (auto p:route) {
            assert(std::abs(p.first-previous.first)+std::abs(p.second-previous.second)==1);
            assert(p.second==0 || p.first==2);
            previous=p;
        }
        assert(route.back()==astar.back());
        int emitted=0;
        assert(!BgEmitInteriorSubtileRoute(201,200,[&](int,bool){++emitted;}));
        assert(!BgEmitInteriorSubtileRoute(0,200,[&](int,bool){++emitted;}));
        assert(emitted==0);
        assert(BgEmitInteriorSubtileRoute(200,200,[&](int,bool){++emitted;}));
        assert(emitted==200);
    }

    assert(BgUpperFloorCentreNavigation(5191,8,12));
    assert(!BgUpperFloorCentreNavigation(5190,8,12));
    assert(!BgUpperFloorCentreNavigation(5191,8,11));
    assert(!BgUpperFloorCentreNavigation(5136,8,12));
    assert(BgStaticClearancePassable(true,9.4f,30));
    assert(!BgStaticClearancePassable(false,9.4f,30));
    assert(!BgStaticClearancePassable(true,0,30));
    assert(!BgStaticClearancePassable(true,std::numeric_limits<float>::infinity(),30));
    assert(BgStaticClearancePassable(false,40,30));
    assert(BgStaticClearancePassable(false,0,0));
    assert(BgAutoIdentifyConsumable(true,true,false));
    assert(BgAutoIdentifyConsumable(true,false,true));
    assert(!BgAutoIdentifyConsumable(true,false,false));
    assert(!BgAutoIdentifyConsumable(false,true,true));
    for (bool alwaysRun:{false,true}) for (bool ctrl:{false,true}) for (bool num:{false,true}) {
        assert(bgmovement::Run(0,alwaysRun,30,ctrl,num)==ctrl);
        assert(bgmovement::Run(1,alwaysRun,30,ctrl,num)==!ctrl);
    }
    assert(bgmovement::BaseRun(-1,true,0));
    assert(!bgmovement::BaseRun(-1,true,10));
    assert(bgmovement::Run(-1,false,0,false,true));
    assert(!bgmovement::Run(-1,false,10,false,true));
    assert(BgNpcMotionOwner(0x42474E00u,false,true));
    assert(BgNpcMotionOwner(0x42474E2Bu,false,true));
    assert(!BgNpcMotionOwner(0x42474E01u,true,true)); // recruited/controlled party member
    assert(!BgNpcMotionOwner(0x42474301u,false,true)); // container is never an actor
    assert(!BgNpcMotionOwner(0,false,true));
    assert(!BgNpcMotionOwner(0x42474E01u,false,false));
    // Regression: visible projected d20 faces used to wind backwards and were
    // culled by the native GUI material, leaving only numbers/edge lines.
    for(int value=1;value<=20;++value) {
        auto pose=bgdice::Landing(value);
        for(auto &indices:bgdice::IcoF) {
            auto a=bgdice::Rotate(bgdice::IcoV[indices[0]],pose.yaw,pose.pitch);
            auto b=bgdice::Rotate(bgdice::IcoV[indices[1]],pose.yaw,pose.pitch);
            auto c=bgdice::Rotate(bgdice::IcoV[indices[2]],pose.yaw,pose.pitch);
            auto normal=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
            if(normal<=0.0001f) continue;
            auto triangle=bgdice::ClockwiseTriangle({a.x,-a.y},{b.x,-b.y},{c.x,-c.y});
            assert(bgdice::ScreenArea(triangle[0],triangle[1],triangle[2])>0);
        }
    }
    DoorSecurity target;target.locked=true;target.trapped=true;target.searchDc=21;
    auto neighbour=target;
    assert(!SearchObjectTrap(target,false,true).found);
    assert(!SearchObjectTrap(target,true,false).found);
    auto found=SearchObjectTrap(target,true,true);assert(found.found && found.locked && found.trapped);
    assert(!target.found && !neighbour.found); // only the returned target is committed
    assert(!SearchObjectTrap(DoorSecurity{},true,true).found);
    TestManualCheckLifecycle();
    for(int number=1;number<=20;++number) assert(bgdice::FrontNumber(bgdice::Landing(number))==number);
    for(int bank:{2100,17500,18400,18600,7100,9300,9400,9500,16300,22100,19600,29000}) {assert(BgChainmailAppearance(bank,true)==18400);assert(BgChainmailAppearance(bank,false)==bank);}
    assert(BgChainmailAppearance(1500,true)==1500);
    assert(BgChainmailAppearance(7600,true)==7600); // banded armour, not chain
    assert(BgChainmailAppearance(4200,true)==4200); // helmet, not torso
    assert(BgChainmailAppearance(20900,true,6296)==18400);
    assert(BgChainmailAppearance(20900,true,6341)==18400);
    assert(BgChainmailAppearance(7600,true,6301)==18400);
    assert(BgChainmailAppearance(20900,true,9999)==20900);
    assert(BgChainmailAppearance(1500,true,6296)==1500);
    assert(BgOccludedVisible(1,true));  // controlled actor behind a GPU source mask
    assert(!BgOccludedVisible(1,false)); // ordinary object still needs native occlusion
    assert(!BgOccludedVisible(0,true)); // no fog visibility bypass
    assert(!BgOccludedVisible(0xB0,true));
    assert(BgOccludedVisible(0xB1,false));
    assert(BgOccludedPass(true,true));
    assert(!BgOccludedPass(true,false)); // no second pass against the actor's own depth
    assert(!BgOccludedPass(false,true));
    assert(BgOccludedPass(false,false));
	bgskillrules::SearchAttempt attempt;
	attempt.Start(1000);assert(!attempt.Poll(3999));assert(attempt.Poll(4000));assert(!attempt.Poll(5000));
	attempt.Start(0);assert(!attempt.Poll(6000));
	attempt.Start(10);attempt.Cancel();assert(!attempt.Poll(3010));
	attempt.Start(0xFFFFFF00);assert(attempt.Poll(uint32_t(0xFFFFFF00u+3000)));
	assert(bgskillrules::InSearchArea(0,0,240,0));
	assert(!bgskillrules::InSearchArea(0,0,240.01f,0));
	assert(!bgskillrules::InSearchArea(0,0,300,300));
	assert(bgskillrules::DisableFailureDelta(1,-1) == -10);
	assert(bgskillrules::DisableFailureDelta(2,-20) == -3);
	for (int quality=0; quality<5; ++quality) {
		uint32_t saved=0;
		for (int n=1; n<=quality+2; ++n) {
			saved=bgskillrules::RecordToolRoll(saved,true);
			assert(bgskillrules::ToolBreaks(saved,quality) == (n==quality+2));
		}
		assert(!bgskillrules::ToolBreaks(bgskillrules::RecordToolRoll(saved,false),quality));
	}
	assert(bgskillrules::ToolStreak(1234) == 0);
	TestSourceContainerRegions();
	TestSelectedArrivalPlanning();
	TestContextAndScenes();
	TestClickTravel();
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
	assert(bgscenecombatpolicy::Join(5195,5195,false));
    assert(!bgscenecombatpolicy::Join(5195,5136,false));
    assert(!bgscenecombatpolicy::Join(5195,5195,true));
    assert(!bgscenecombatpolicy::Target(5195,5195,true));
    assert(!bgscenecombatpolicy::Target(5195,5136,false));
    assert(bgscenecombatpolicy::Free(false,true,false,5136,5195));
    assert(bgscenecombatpolicy::Free(false,false,true,5195,5195));
    assert(!bgscenecombatpolicy::Free(true,true,false,5195,5195));
    assert(bgscenecombatpolicy::SameScene(5195,5195));
    assert(!bgscenecombatpolicy::SameScene(5195,5136));
    assert(!bgscenecombatpolicy::SameScene(0,0));
    using bgengagement::AutoJoin;
    // Sharing a room alone cannot enlist a hidden, uninvolved party member.
    assert(!AutoJoin({true,false,false,false,false,false}));
    assert(AutoJoin({true,false,false,false,true,false})); // detected
    assert(AutoJoin({true,false,false,true,false,false})); // actual enemy focus
    assert(!AutoJoin({true,false,false,false,true,true})); // healing is still committed
    assert(AutoJoin({true,false,false,false,true,false})); // joins after completion
    assert(AutoJoin({true,true,false,false,false,true})); // existing member never leaves
    assert(AutoJoin({true,false,true,false,false,true})); // native initiating pair
    assert(!AutoJoin({false,false,true,true,true,false})); // dead/foreign scene excluded
    using JS=bgengagement::JoinSelection;
    assert(bgengagement::SelectionAfterJoin(false,false)==JS::Keep);
    assert(bgengagement::SelectionAfterJoin(false,true)==JS::Keep);
    assert(bgengagement::SelectionAfterJoin(true,true)==JS::FreeActors);
    assert(bgengagement::SelectionAfterJoin(true,false)==JS::CurrentTurn);
    assert(!bgengagement::KeepSelection(true,true,false)); // current fighter is exclusive
    assert(bgengagement::KeepSelection(true,true,true)); // detached group remains possible
    assert(!bgengagement::KeepSelection(false,true,true));
    assert(!bgengagement::KeepSelection(true,false,true));

    {
        using bgengagement::Senses;
        using bgengagement::Detected;
        // distance, sight, sneaking, night; spot, listen, hide, move silently; rolls in the same order
        assert(Detected(Senses{35,true,false,false,0,0,0,0,1,1,20,20})); // open hero in view within 40 ft
        assert(!Detected(Senses{41,true,false,false,20,20,0,0,20,20,1,1})); // beyond 40 ft nobody is noticed
        assert(Detected(Senses{20,false,false,false,0,0,0,0,1,12,1,1})); // heard: 12-2 meets DC 10
        assert(!Detected(Senses{30,false,false,false,0,0,0,0,1,12,1,1})); // 12-3 misses DC 10
        assert(Detected(Senses{10,true,true,false,5,0,4,0,15,1,10,1})); // spot 15+5-1=19 beats hide 10+4
        assert(!Detected(Senses{10,true,true,false,5,0,4,0,10,1,10,1})); // 14 ties hide 14: the hero stays hidden
        assert(!Detected(Senses{10,true,true,true,5,0,4,0,15,1,10,1})); // night: hide 10+4+5 ties spot 19
        assert(Detected(Senses{20,false,true,false,20,3,0,2,20,15,1,13})); // unseen: listen 15+3-2=16 beats 13+2
        assert(!Detected(Senses{20,false,true,false,20,3,0,2,20,15,1,14})); // 16 ties move silently 16; no sight, no spot
        assert(!Detected(Senses{10,true,true,false,5,0,4,0,15,1,10,1,true})); // behind an unalerted enemy: 19-5 ties 14
        assert(Detected(Senses{10,true,true,false,5,0,4,0,20,1,10,1,true})); // 24-5=19 still beats 14
        assert(Detected(Senses{20,false,true,false,0,3,0,2,1,15,1,13,true})); // Listen ignores facing
        assert(Detected(Senses{35,true,false,false,0,0,0,0,1,1,20,20,true})); // an open hero is seen from behind too
        using bgengagement::Behind;
        assert(!Behind(0,0) && Behind(0,3.14159) && !Behind(1,1.5));
        assert(!Behind(0.1,6.2) && !Behind(6.2,0.1)); // across the 0/2pi seam: in front
        assert(Behind(0,1.5708+0.01) && !Behind(0,1.5708-0.01));
    }
    bgengagement::DetectionCache<int> perception;
    int checks=0;
    auto missed=[&]() {++checks;return false;};
    assert(!perception.Observe(1,2,100,1,missed));
    for (uint32_t now=350;now<6100;now+=250)
        assert(!perception.Observe(1,2,now,1,missed));
    assert(checks==1); // render pulses do not retry Spot/Listen until the next round
    assert(perception.Observe(1,2,6100,1,[&]() {++checks;return true;}));
    assert(checks==2);
    assert(!perception.Observe(3,2,6100,1,missed)); // observers do not share results
    assert(!perception.Observe(1,2,6100,5,missed)); // changed concealment/invisibility
    perception.Forget(1,2);
    assert(perception.Observe(1,2,6101,5,[]() {return true;}));
    perception.Clear();
    assert(!perception.Observe(1,2,6102,5,missed)); // load/new encounter resets cache
    const auto nearWrap=(std::numeric_limits<uint32_t>::max)()-100;
    assert(!perception.Observe(4,2,nearWrap,1,missed));
    int beforeWrap=checks;
    assert(!perception.Observe(4,2,50,1,missed));
    assert(checks==beforeWrap); // Windows millisecond counter wrap is harmless
    assert(perception.Observe(4,2,6000,1,[]() {return true;}));
    BgSequenceReservations<int> reservations;
    int battle=1,freeAction=2;
    assert(!reservations.Contains(&battle));
    {
        BgSequenceReservations<int>::Scope savedBattle(reservations,&battle);
        assert(reservations.Contains(&battle));
        assert(!reservations.Contains(&freeAction));
        try {
            BgSequenceReservations<int>::Scope callback(reservations,&freeAction);
            assert(reservations.Contains(&battle) && reservations.Contains(&freeAction));
            throw 8;
        } catch (int) {}
        assert(reservations.Contains(&battle) && !reservations.Contains(&freeAction));
        {BgSequenceReservations<int>::Scope nested(reservations,&battle);}
        assert(reservations.Contains(&battle));
    }
    assert(!reservations.Contains(&battle));
    BgActorContext<int> contexts;
    bool joined=false;
    auto freeActor=[&](int actor) {return actor==2 && !joined;};
    assert(contexts.Combat(true,freeActor)); // the scheduler sees the encounter
    {
        BgActorContext<int>::Scope detached(contexts,2);
        assert(!contexts.Combat(true,freeActor)); // ordinary realtime rules
        {
            BgActorContext<int>::Scope combatant(contexts,1);
            assert(contexts.Combat(true,freeActor)); // nested battle callback
        }
        assert(!contexts.Combat(true,freeActor));
        joined=true;
        assert(contexts.Combat(true,freeActor)); // membership rechecked, never cached
        joined=false;
        assert(!contexts.Combat(false,freeActor)); // encounter ended during callback
        try {
            BgActorContext<int>::Scope callback(contexts,1);
            throw 7;
        } catch (int) {}
        assert(!contexts.Combat(true,freeActor)); // unwind restores actor
    }
    assert(contexts.Combat(true,freeActor));
    assert(!contexts.Combat(false,freeActor));
    {
        // A waits in encounter 1 while B opens encounter 2; each keeps its own turn state.
        struct Turn {int round,actor,budget;};
        bgencounters::Registry<int,Turn> encounters;
        Turn first{3,1,2};
        auto one=encounters.Open();
        assert(one && !encounters.Park(one,{},first)); // no empty encounter
        assert(!encounters.Park(one,{0,7},first)); // null actor never owned
        assert(encounters.Park(one,{1,7},first));
        assert(encounters.Owns(1) && encounters.Owns(7) && !encounters.Owns(2));
        assert(encounters.Owner(7)==one && encounters.Owner(2)==0);
        auto two=encounters.Open();
        assert(two!=one);
        assert(!encounters.Park(two,{2,7},Turn{1,2,4})); // one actor, one encounter
        assert(!encounters.Park(one,{3},Turn{1,3,4})); // ids never fork
        assert(encounters.Size()==1 && !encounters.Owns(2));
        // Showing encounter 1 parks encounter 2 with its own round and budget.
        decltype(encounters)::Parked shown;
        assert(encounters.Take(one,shown) && !encounters.Owns(1));
        assert(shown.state.round==3 && shown.state.actor==1 && shown.state.budget==2);
        assert(encounters.Park(two,{2,8},Turn{1,2,4}));
        assert(!encounters.Take(one,shown)); // taken once, not duplicated
        assert(encounters.Park(one,shown.members,shown.state));
        // Detection scans every waiting fight's members without taking any.
        std::vector<int> seen;
        encounters.ForEach([&](const decltype(encounters)::Parked &entry) {seen.insert(seen.end(),entry.members.begin(),entry.members.end());});
        assert((seen==std::vector<int>{2,8,1,7}) && encounters.Size()==2);
        encounters.Forget(8);
        assert(!encounters.Owns(8) && encounters.Owns(2));
        assert(encounters.TakeLatest(shown) && shown.id==one && shown.state.budget==2); // latest parked resumes first
        assert(encounters.TakeLatest(shown) && shown.id==two && shown.members==std::vector<int>{2});
        assert(!encounters.TakeLatest(shown) && encounters.Size()==0);
        encounters.Park(encounters.Open(),{4},first);
        encounters.Clear();
        assert(!encounters.Owns(4));
        using bgencounters::CanPark;
        assert(CanPark({true,false,false,false,false,false}));
        assert(!CanPark({false,false,false,false,false,false})); // AI turn finishes first
        assert(!CanPark({true,true,false,false,false,false})); // animation or projectile pending
        assert(!CanPark({true,false,true,false,false,false})); // simultaneous actions pending
        assert(!CanPark({true,false,false,true,false,false})); // interrupt or readied action pending
        assert(!CanPark({true,false,false,false,true,false})); // a target picker is open
        assert(!CanPark({true,false,false,false,false,true})); // dialogue owns the encounter
        encounters.Park(encounters.Open(),{5},first);
        encounters.ForEach([](decltype(encounters)::Parked &entry) {++entry.state.round;}); // a waiting fight keeps time
        assert(encounters.Take(encounters.Owner(5),shown) && shown.state.round==4);
        using bgencounters::OnHeroTurn;using bgencounters::DownedTurn;
        assert(OnHeroTurn(false,false,false)==DownedTurn::Act);
        assert(OnHeroTurn(true,true,true)==DownedTurn::Skip); // another hero of the fight acts
        assert(OnHeroTurn(true,false,true)==DownedTurn::Hold); // the fight goes on without its heroes
        assert(OnHeroTurn(true,false,false)==DownedTurn::Skip); // the whole party is down: native rules
        bgencounters::RoundClock clock;clock.Start(1000);
        assert(clock.Advance(1250,true)==0);
        unsigned rounds=0;
        for (uint32_t now=1500;now<=13000;now+=250) rounds+=clock.Advance(now,true);
        assert(rounds==2); // 12 seconds are two rounds
        assert(clock.Advance(60000,true)==0); // a long gap is one step, not eight rounds
        for (uint32_t now=60250;now<=90000;now+=250) assert(clock.Advance(now,false)==0); // paused in dialogue
        rounds=0;
        for (uint32_t now=90250;now<=95250;now+=250) rounds+=clock.Advance(now,true);
        assert(rounds==1); // 1 s before the pause plus 5 s after it
        {
            struct Id {uint16_t kind;uint32_t body[5];};
            bgencounters::Writer out;
            out.Put(uint32_t(1));out.Put(Id{2,{7,8,9,10,11}});out.Put(uint8_t(1));
            bgencounters::Reader in(out.Bytes());
            uint32_t version=0;Id id{};uint8_t flag=0;
            assert(in.Get(version) && version==1 && in.Get(id) && id.kind==2 && id.body[4]==11 && in.Get(flag) && flag==1);
            assert(in.Done() && !in.Get(version)); // nothing past the end
            std::vector<uint8_t> cut(out.Bytes().begin(),out.Bytes().end()-3);
            bgencounters::Reader truncated(cut);
            assert(truncated.Get(version) && !truncated.Get(id)); // a truncated record is rejected
            bgencounters::Registry<int,Turn> saved;
            saved.Seed(5);
            assert(saved.Open()==6 && saved.Last()==6); // ids continue after a load
            saved.Seed(2);
            assert(saved.Last()==6);
        }
        using bgencounters::UnattendedEnemy;using bgencounters::EnemyMove;
        assert(UnattendedEnemy(false,false,true)==EnemyMove::Hold); // nobody to finish
        assert(UnattendedEnemy(true,true,true)==EnemyMove::Hold); // a conscious hero is near: no coup de grace
        assert(UnattendedEnemy(true,false,false)==EnemyMove::Approach);
        assert(UnattendedEnemy(true,false,true)==EnemyMove::Finish);
        bgencounters::Opening<int> opening;
        assert(!opening.Unseen(1,2,0));
        opening.Note(1,2,true,1000);
        assert(opening.Unseen(1,2,3000)); // every attack of that action
        assert(!opening.Unseen(1,3,3000) && !opening.Unseen(4,2,3000)); // only that attacker and target
        assert(!opening.Unseen(1,2,7000)); // not a later round
        opening.Note(1,2,false,8000);
        assert(!opening.Unseen(1,2,8000)); // the next action was seen
        opening.Note(1,2,true,9000);opening.Clear();
        assert(!opening.Unseen(1,2,9000));
    }
    std::cout << "Original door registry tests passed\n";
}
