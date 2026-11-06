#include "../../TemplePlus/graphics/originaldoorstate.h"
#include "../../TemplePlus/graphics/originaldoorsave.h"
#include "../../TemplePlus/graphics/originaldooruse.h"
#include "../../TemplePlus/graphics/originaltravel.h"
#include "../../TemplePlus/graphics/originaldoorcontext.h"
#include "../../TemplePlus/graphics/sourcecontainers.h"
#include "../../TemplePlus/worldviewzoom.h"
#include "../../TemplePlus/ui/bgskillrules.h"
#include "../../TemplePlus/bgcheckstate.h"
#include "../../TemplePlus/bgappearance.h"
#include "../../TemplePlus/bgmovementrules.h"
#include "../../TemplePlus/bgconsumables.h"
#include "../../TemplePlus/ui/bgdicegeometry.h"
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

int main() {
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
    for(int bank:{2100,17500,18400,18600,7100,9300,9400,9500,16300,22100}) {assert(BgChainmailAppearance(bank,true)==18400);assert(BgChainmailAppearance(bank,false)==bank);}
    assert(BgChainmailAppearance(1500,true)==1500);
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
	std::cout << "Original door registry tests passed\n";
}
