#include "stdafx.h"
#include "mapterrain.h"
#include "maps.h"
#include "gameview.h"
#include "party.h"
#include "critter.h"
#include "d20.h"
#include "dispatcher.h"
#include "combat.h"
#include "float_line.h"
#include "raycast.h"
#include "animgoals/anim.h"
#include "gamesystems/gamesystems.h"
#include "gamesystems/mapsystem.h"
#include "gamesystems/legacymapsystems.h"
#include "gamesystems/objects/objsystem.h"
#include "config/config.h"
#include <graphics/device.h>
#include <graphics/shaperenderer2d.h>
#include "python/python_integration_obj.h"
#include "python/python_object.h"

namespace {
void ContextMessage(objHndl actor,const char *message) {
	if (actor) floatSys.floatMesLine(actor,1,FloatLineColor::LightBlue,message);
}
bool PolygonNear(const OriginalDoorPolygon &polygon,int x,int y,double reach) {
	if (DoorPolygonContains(polygon,x,y)) return true;
	for (size_t i=0;i<polygon.size();++i) {
		auto a=polygon[i],b=polygon[(i+1)%polygon.size()];
		double dx=b.x-a.x,dy=b.y-a.y,len=dx*dx+dy*dy;
		auto t=len ? (std::max)(0.0,(std::min)(1.0,((x-a.x)*dx+(y-a.y)*dy)/len)):0.0;
		if (std::hypot(x-a.x-t*dx,y-a.y-t*dy)<=reach) return true;
	}
	return false;
}
}

bool TerrainSystem::ContextTravel(const OriginalTravelMarker &marker) const {
	return mSceneRoutes.count(marker.id)!=0;
}
std::vector<DoorContextAction> TerrainSystem::ContextActions(const OriginalTravelMarker &marker) const {
	auto definition=mDoorContexts.find({marker.gateMap,marker.gateId});
	if (definition==mDoorContexts.end()) throw std::invalid_argument("door context is not bound");
	return DoorContextActions(GetOriginalDoorOpenOnMap(marker.gateMap,marker.gateId),definition->second.security,
		ContextTravel(marker),!definition->second.textures[static_cast<size_t>(DoorContextAction::Disarm)].empty());
}
void TerrainSystem::SpringDoorTrap(int mapId,const std::string &id,objHndl actor) {
	auto &security=mDoorContexts.at({mapId,OriginalDoorState::NormalizeId(id)}).security;
	security=DoorTrapSprung(security);++mDoorUseRevision;
	if (actor) floatSys.floatMesLine(actor,1,FloatLineColor::Red,"Trap triggered!");
	logger->info("Door trap sprung: map={} id={} actor={}",mapId,id,actor.handle);
	// The mod decides the effect with native D20 rules; no handler means no damage.
	auto args=Py_BuildValue("(isN)",mapId,id.c_str(),PyObjHndl_Create(actor));
	auto result=pythonObjIntegration.ExecuteScript("templeplus.savehook","door_trap",args);
	Py_XDECREF(result);Py_DECREF(args);
}
void TerrainSystem::BindDoorContext(int mapId,const std::string &id,DoorContextDefinition definition) {
	ValidateDoorSecurity(definition.security);
	if (!mOriginalDoors.Find(mapId,id) || !mOriginalDoors.HasNavigation(mapId,id))
		throw std::invalid_argument("bind door/navigation before context actions");
	for (size_t index=0;index<definition.textures.size();++index) {
		const auto &path=definition.textures[index];
		if (path.empty() && index==static_cast<size_t>(DoorContextAction::Disarm)) continue; // legacy five-icon profile
		if (path.compare(0,23,"art/bg2toee/ui/actions/") || path.find("..")!=std::string::npos
			|| path.find(':')!=std::string::npos || path.size()>240 || path.size()<4 || path.substr(path.size()-4)!=".png")
			throw std::invalid_argument("context icons must be PNG under art/bg2toee/ui/actions/");
		auto texture=mDevice.GetTextures().Resolve(path,false);
		if (!texture || !texture->IsValid() || !texture->GetResourceView()) throw std::invalid_argument("cannot load context icon: "+path);
	}
	mDoorContexts[{mapId,OriginalDoorState::NormalizeId(id)}]=std::move(definition);
	CancelOriginalDoorUse();++mDoorUseRevision;
}
DoorSecurity TerrainSystem::GetDoorSecurity(int mapId,const std::string &id) const {
	auto entry=mDoorContexts.find({mapId,OriginalDoorState::NormalizeId(id)});
	if (entry==mDoorContexts.end()) throw std::invalid_argument("door context is not bound");
	return entry->second.security;
}
void TerrainSystem::BindContextOutline(int mapId,const std::string &route,OriginalDoorPolygon outline) {
	ValidateDoorPolygon(outline);
	auto key=std::make_pair(mapId,OriginalDoorState::NormalizeId(route));
	if (!mTravelMarkers.count(key)) throw std::invalid_argument("bind travel marker before context outline");
	mContextOutlines[key]=std::move(outline);ClearDoorContextHover();
}
void TerrainSystem::ClearDoorContext() {
	if (mSavedSceneLimits && maps.GetCurrentMapId()==5136) {
		temple::GetRef<int64_t>(0x10307340)=(*mSavedSceneLimits)[0];
		temple::GetRef<int64_t>(0x10307368)=(*mSavedSceneLimits)[1];
		temple::GetRef<int64_t>(0x103072F0)=(*mSavedSceneLimits)[2];
		temple::GetRef<int64_t>(0x10307358)=(*mSavedSceneLimits)[3];
	}
	mSavedSceneLimits.reset();mFocusedScene=0;mSceneAreas.clear();mSceneRoutes.clear();
	mDoorContexts.clear();mContextOutlines.clear();ClearDoorContextHover();CancelOriginalDoorUse();++mDoorUseRevision;
}
bool TerrainSystem::HasDoorContext() const {
	for (const auto &entry:mTravelMarkers) if (entry.first.first==maps.GetCurrentMapId()
		&& mDoorContexts.count({entry.second.gateMap,entry.second.gateId})) return true;
	return false;
}
std::vector<uint64_t> TerrainSystem::ContextSelection() const {
	std::vector<uint64_t> result;
	for (uint32_t i=0;i<party.CurrentlySelectedNum();++i) {
		auto actor=party.GetCurrentlySelected(static_cast<int>(i));
		if (actor && gameSystems->GetObj().IsValidHandle(actor) && party.IsInParty(actor)
			&& !critterSys.IsDeadOrUnconscious(actor)) result.push_back(actor.handle);
	}
	return result;
}
std::vector<OriginalDoorUsePoint> TerrainSystem::ContextLayout(const OriginalTravelMarker &marker,size_t count) const {
	auto zoom=gameView->GetWorldZoom();auto view=gameView->GetWorldViewport();int ox,oy;GetTerrainOrigin(ox,oy);
	double half=26/zoom,step=54/zoom;
	auto x=marker.x-(count-1)*step/2;
	x=(std::max)(view.left-ox+half,(std::min)(x,view.right-ox-half-(count-1)*step));
	auto y=(std::max)(view.top-oy+half,(std::min)(marker.y,view.bottom-oy-half));
	std::vector<OriginalDoorUsePoint> result;
	for (size_t i=0;i<count;++i) result.push_back({x+i*step,y});
	return result;
}
std::string TerrainSystem::UpdateDoorContextHover(int screenX,int screenY) {
	auto now=timeGetTime();int ox,oy;GetTerrainOrigin(ox,oy);int x=screenX-ox,y=screenY-oy;
	mHoveredContextAction=-1;
	// Keep a visible palette stable while moving from its door onto its buttons.
	if (mContextHover.Visible(now)) {
		auto entry=mTravelMarkers.find({maps.GetCurrentMapId(),mContextHover.Target()});
		if (entry!=mTravelMarkers.end()) {
			const auto &m=entry->second;
			auto actions=ContextActions(m);
			auto positions=ContextLayout(m,actions.size());
			for (size_t i=0;i<positions.size();++i) if (std::abs(x-positions[i].x)*gameView->GetWorldZoom()<=25
				&& std::abs(y-positions[i].y)*gameView->GetWorldZoom()<=25) {
				mContextHover.Update(m.id,now);mHoveredContextAction=static_cast<int>(actions[i]);
				return "!"+std::to_string(mHoveredContextAction)+"|"+m.id;
			}
		}
	}
	std::string target;auto door=mOriginalDoors.HitTestStable(maps.GetCurrentMapId(),x,y);
	for (const auto &entry:mTravelMarkers) {
		if (entry.first.first!=maps.GetCurrentMapId() || !mDoorContexts.count({entry.second.gateMap,entry.second.gateId})) continue;
		auto scene=mSceneRoutes.find(entry.first.second);
		if (scene!=mSceneRoutes.end() && !SceneContains(mSceneAreas.at(scene->second.source),x,y)) continue;
		auto outline=mContextOutlines.find(entry.first);
		if ((!door.empty() && entry.second.gateId==door && mOriginalDoors.Outline(entry.second.gateMap,door))
			|| (outline!=mContextOutlines.end() && PolygonNear(outline->second,x,y,6/gameView->GetWorldZoom()))) {
			if (!target.empty()) {target.clear();break;}target=entry.first.second;
		}
	}
	mContextHover.Update(target,now);
	return target.empty() ? std::string():"#"+target;
}
void TerrainSystem::RequestDoorContextAction(const std::string &route,int rawAction) {
	if (rawAction<0 || rawAction>=static_cast<int>(DoorContextActionCount)) throw std::invalid_argument("invalid context action");
	auto entry=mTravelMarkers.find({maps.GetCurrentMapId(),OriginalDoorState::NormalizeId(route)});
	if (entry==mTravelMarkers.end()) throw std::invalid_argument("context route missing");
	const auto &m=entry->second;
	auto action=static_cast<DoorContextAction>(rawAction);auto open=GetOriginalDoorOpenOnMap(m.gateMap,m.gateId);
	auto allowed=ContextActions(m);
	if (std::find(allowed.begin(),allowed.end(),action)==allowed.end()) return; // stale state/icon cannot become a different action
	auto selection=ContextSelection();
	if (selection.empty()) throw std::invalid_argument("select at least one conscious party member");
	auto scene=mSceneRoutes.find(m.id);
	if (scene!=mSceneRoutes.end()) for (auto actor:selection) if (SceneAreaForActor(actor)!=scene->second.source)
		throw std::invalid_argument("select characters in the same visible area");
	std::vector<DoorSkillCandidate> candidates;
	auto skill=action==DoorContextAction::PickLock ? skill_open_lock
		: action==DoorContextAction::Disarm ? skill_disable_device : skill_search;
	auto skillAction=action==DoorContextAction::PickLock || action==DoorContextAction::Search || action==DoorContextAction::Disarm;
	for (auto handle:selection) {
		objHndl actor{handle};auto pos=objects.GetLocationFull(actor).ToInches2D();
		auto actualSkill=skill;auto bonus=0;auto eligible=true;
		if (skillAction) {
			auto swap=d20Sys.D20QueryPython(actor,"Skill Swap",skill);
			if (swap>0 && swap<=42) actualSkill=static_cast<SkillEnum>(swap-1);
			auto props=skillSys.skillPropsTable[actualSkill];
			eligible=skillSys.IsEnabled(actualSkill) && (props.unskilledUseAllow || critterSys.SkillBaseGet(actor,actualSkill)>0);
			if (props.stat==stat_intelligence && d20Sys.d20Query(actor,DK_QUE_CannotUseIntSkill)) eligible=false;
			if (props.stat==stat_charisma && d20Sys.d20Query(actor,DK_QUE_CannotUseChaSkill)) eligible=false;
			bonus=dispatch.dispatch1ESkillLevel(actor,actualSkill,nullptr,objHndl::null,
				action==DoorContextAction::Search ? SRF_Trap_Search:SRF_Unk_1);
		}
		candidates.push_back({handle,bonus,OriginalDoorUseDistance({pos.x,pos.y},m.approach.points.front()),eligible});
	}
	auto handle=BestDoorSkillCandidate(candidates);
	if (!handle) throw std::invalid_argument("no selected character can use this skill");
	if (action==DoorContextAction::Travel) {RequestOriginalTravel(route,handle);return;}
	CancelOriginalDoorUse();objHndl actor{handle};auto pos=objects.GetLocationFull(actor).ToInches2D();
	mDoorUseQueue.Begin(m.mapId,m.gateId,handle,mDoorUseRevision,timeGetTime(),open,m.approach,{pos.x,pos.y},60000);
	mContextRequest=ContextRequest{m.gateMap,m.gateId,m.id,action,selection};
	logger->info("Door context request: route={} action={} actor={} selected={}",m.id,rawAction,handle,selection.size());
	AdvanceTime(timeGetTime());
	if (mDoorUseQueue.Pending()) {
		auto target=mDoorUseQueue.Pending()->target;
		if (!gameSystems->GetAnim().PushRunNearTile(actor,LocAndOffsets::FromInches(float(target.x),float(target.y)),4)) {
			CancelOriginalDoorUse();throw std::invalid_argument("cannot approach this door");
		}
	}
}
void TerrainSystem::ApplyDoorContext(const ContextRequest &request,uint64_t handle) {
	if (!SameDoorSelection(request.selection,ContextSelection())) throw std::invalid_argument("selection changed; door action cancelled");
	auto &security=mDoorContexts.at({request.gateMap,request.gate}).security;objHndl actor{handle};
	if (request.action==DoorContextAction::Open || request.action==DoorContextAction::Close) {
		if (security.locked && request.action==DoorContextAction::Open) {ContextMessage(actor,"The door is locked");return;}
		SetOriginalDoorOpen(request.gate,request.action==DoorContextAction::Open);
		if (request.action==DoorContextAction::Open && security.trapped) SpringDoorTrap(request.gateMap,request.gate,actor);
		return;
	}
	if (request.action==DoorContextAction::PickLock) {
		if (!security.locked) return;
		if (!security.pickable) {ContextMessage(actor,"This lock requires a key");return;}
		int delta=0;auto success=skillSys.SkillRoll(actor,skill_open_lock,security.lockDc,&delta,SRF_Unk_1)!=FALSE;
		if (success) {security.locked=false;++mDoorUseRevision;}
		ContextMessage(actor,success ? "Lock opened":"Failed to pick the lock");
		logger->info("Door skill roll: actor={} skill=OpenLock dc={} delta={} success={}",handle,security.lockDc,delta,success);
	} else if (request.action==DoorContextAction::Search) {
		// 3.5: traps above DC 20 need trapfinding; the roll is still made and logged.
		int delta=0;auto success=skillSys.SkillRoll(actor,skill_search,security.searchDc,&delta,SRF_Trap_Search)!=FALSE;
		if (security.searchDc>20 && !d20Sys.d20Query(actor,DK_QUE_Critter_Can_Find_Traps)) success=false;
		if (success && security.trapped && !security.found) {security.found=true;++mDoorUseRevision;}
		ContextMessage(actor,success && security.trapped ? "A trap was found":"No trap was found");
		logger->info("Door skill roll: actor={} skill=Search dc={} delta={} success={}",handle,security.searchDc,delta,success);
	} else if (request.action==DoorContextAction::Disarm) {
		if (!security.trapped || !security.found) return;
		int delta=0;auto success=skillSys.SkillRoll(actor,skill_disable_device,security.disarmDc,&delta,SRF_Unk_1)!=FALSE;
		logger->info("Door skill roll: actor={} skill=DisableDevice dc={} delta={} success={}",handle,security.disarmDc,delta,success);
		if (success) {security.trapped=false;security.found=false;++mDoorUseRevision;ContextMessage(actor,"Trap disarmed");}
		else if (DoorDisarmSpringsTrap(delta)) SpringDoorTrap(request.gateMap,request.gate,actor);
		else ContextMessage(actor,"Failed to disarm the trap");
	}
}
void TerrainSystem::RenderDoorContext() {
	auto now=timeGetTime();if (!mContextHover.Active(now)) return;
	auto entry=mTravelMarkers.find({maps.GetCurrentMapId(),mContextHover.Target()});if (entry==mTravelMarkers.end()) return;
	const auto &m=entry->second;auto definition=mDoorContexts.find({m.gateMap,m.gateId});if (definition==mDoorContexts.end()) return;
	int ox,oy;GetTerrainOrigin(ox,oy);auto outline=mContextOutlines.find(entry->first);
	if (outline!=mContextOutlines.end()) {
		std::vector<gfx::Line2d> lines;const auto &poly=outline->second;
		for (size_t i=0;i<poly.size();++i) lines.emplace_back(XMFLOAT2(float(ox+poly[i].x),float(oy+poly[i].y)),
			XMFLOAT2(float(ox+poly[(i+1)%poly.size()].x),float(oy+poly[(i+1)%poly.size()].y)),XMCOLOR(0.2f,0.65f,1.0f,0.9f));
		mShapeRenderer.DrawLines(lines);
	}
	if (!mContextHover.Visible(now)) return;
	auto actions=ContextActions(m);
	auto layout=ContextLayout(m,actions.size());auto size=48/gameView->GetWorldZoom();
	for (size_t i=0;i<actions.size();++i) {
		auto action=static_cast<int>(actions[i]);const auto &path=definition->second.textures[action];
		if (path.empty()) continue;auto texture=mDevice.GetTextures().Resolve(path,false);
		if (!texture || !texture->IsValid()) continue;
		auto alpha=mContextHover.Alpha(now)*(mHoveredContextAction==action ? 1.38f:1.0f);
		if (actions[i]==DoorContextAction::PickLock && !definition->second.security.pickable) alpha*=0.4f;
		mShapeRenderer.DrawRectangle(float(ox+layout[i].x-size/2),float(oy+layout[i].y-size/2),size,size,*texture,
			XMCOLOR(1.0f,1.0f,1.0f,alpha));
	}
}

void TerrainSystem::BindSceneArea(OriginalSceneArea area) {
	ValidateOriginalSceneArea(area);
	if (area.nativeMap!=5136 || (area.id!=5136 && area.id!=5190)) throw std::invalid_argument("scene probe restricted to Candlekeep/inn");
	if (area.sourceGround && !mTerrainDirs.count(area.sourceGround)) throw std::invalid_argument("source interior ground is not installed");
	for (const auto &entry:mSceneAreas) if (entry.first!=area.id) {
		const auto &other=entry.second;
		if (area.x<other.x+other.width && other.x<area.x+area.width && area.y<other.y+other.height && other.y<area.y+area.height)
			throw std::invalid_argument("isolated scene areas overlap");
	}
	mSceneAreas[area.id]=area;mFocusedScene=0;CancelOriginalDoorUse();++mDoorUseRevision;
}
void TerrainSystem::BindSceneRoute(const std::string &route,OriginalSceneRoute definition) {
	if (!mSceneAreas.count(definition.source) || !mSceneAreas.count(definition.destination) || definition.source==definition.destination
		|| SceneAreaAt(definition.arrival)!=definition.destination) throw std::invalid_argument("invalid isolated scene route");
	mSceneRoutes[OriginalDoorState::NormalizeId(route)]=definition;CancelOriginalDoorUse();++mDoorUseRevision;
}
int TerrainSystem::SceneAreaAt(OriginalDoorUsePoint point) const {
	auto pixel=SceneCanvasPoint(point.x,point.y);
	for (const auto &entry:mSceneAreas) if (entry.second.nativeMap==maps.GetCurrentMapId() && SceneContains(entry.second,pixel.x,pixel.y)) return entry.first;
	return 0;
}
int TerrainSystem::SceneAreaForActor(uint64_t handle) const {
	objHndl actor{handle};if (!gameSystems->GetObj().IsValidHandle(actor)) return 0;
	auto pos=objects.GetLocationFull(actor).ToInches2D();return SceneAreaAt({pos.x,pos.y});
}
void TerrainSystem::UpdateSceneFocus() {
	if (mSceneAreas.empty() || maps.GetCurrentMapId()!=5136 || gameSystems->GetMap().IsClearingMap()) return;
	auto selected=ContextSelection();if (selected.empty()) return;auto areaId=SceneAreaForActor(selected.front());
	if (!areaId || areaId==mFocusedScene) return;
	if (!mSavedSceneLimits) mSavedSceneLimits=std::array<int64_t,4>{temple::GetRef<int64_t>(0x10307340),temple::GetRef<int64_t>(0x10307368),
		temple::GetRef<int64_t>(0x103072F0),temple::GetRef<int64_t>(0x10307358)};
	const auto &area=mSceneAreas.at(areaId);double left=8428-area.x,right=8428-area.x-area.width;
	double top=-4366-area.y,bottom=-4366-area.y-area.height;
	auto dw=(std::max)(0.0,(config.renderWidth+100-(left-right))/2),dh=(std::max)(0.0,(config.renderHeight+100-(top-bottom))/2);
	temple::GetRef<int64_t>(0x10307340)=int64_t(left+dw);temple::GetRef<int64_t>(0x103072F0)=int64_t(right-dw);
	temple::GetRef<int64_t>(0x10307368)=int64_t(top+dh);temple::GetRef<int64_t>(0x10307358)=int64_t(bottom-dh);
	ClearDoorContextHover();mFocusedScene=areaId;auto loc=objects.GetLocationFull(objHndl{selected.front()}).location;
	gameSystems->GetLocation().CenterOn(loc.locx,loc.locy);
	logger->info("Isolated scene focus: area={} actor={}",areaId,selected.front());
}
bool TerrainSystem::TravelSelectedInScene(uint32_t serial) {
	auto intent=PeekOriginalTravel();if (!intent || intent->serial!=serial) return false;
	auto route=mSceneRoutes.find(intent->id);if (route==mSceneRoutes.end()) return false;
	if (!SameDoorSelection(mTravelSelection,ContextSelection()) || combatSys.isCombatActive()) {CancelOriginalDoorUse();return false;}
	auto selection=mTravelSelection;auto target=route->second.arrival;
	struct Move {objHndl actor;LocAndOffsets before,after;float radius;};std::vector<Move> plan;
	std::vector<SceneArrivalActor> actors;
	for (auto handle:selection) {
		if (SceneAreaForActor(handle)!=route->second.source) {CancelOriginalDoorUse();return false;}
		actors.push_back({handle,objects.GetRadius(objHndl{handle})});
	}
	std::vector<SceneArrivalSpot> spots;
	auto canStand=[&](const SceneArrivalActor &actor,OriginalDoorUsePoint point) {
		if (SceneAreaAt(point)!=route->second.destination) return false;
		auto loc=LocAndOffsets::FromInches(float(point.x),float(point.y));RaycastPacket ray;
		ray.flags=static_cast<RaycastFlags>(StopAfterFirstBlockerFound|StopAfterFirstFlyoverFound|HasRadius|HasSourceObj|ExcludeItemObjects);
		ray.origin=loc;ray.targetLoc=loc;ray.sourceObj=objHndl{actor.id};ray.radius=float(actor.radius);
		return ray.RaycastShortRange()==0;
	};
	if (!PlanSceneArrival(actors,target,canStand,spots)) {
		ContextMessage(objHndl{intent->actor},"Not enough free space for the selected group");CancelOriginalDoorUse();return false;
	}
	for (const auto &spot:spots) {
		objHndl actor{spot.id};
		plan.push_back({actor,objects.GetLocationFull(actor),LocAndOffsets::FromInches(float(spot.point.x),float(spot.point.y)),float(spot.radius)});
	}

	// No object is moved before every arrival position has been checked.
	for (const auto &move:plan) StopDoorMovement(move.actor.handle);
	if (!mTravelReady.Ack(serial)) return false;
	try {for (const auto &move:plan) objects.Move(move.actor,move.after);}
	catch (...) {for (const auto &move:plan) objects.Move(move.actor,move.before);CancelOriginalDoorUse();throw;}
	mTravelSelection.clear();mFocusedScene=0;ClearDoorContextHover();UpdateSceneFocus();
	logger->info("Selected scene travel: route={} destination={} moved={}",intent->id,route->second.destination,plan.size());
	return true;
}
