#pragma once
#include "originaltravel.h"
#include <limits>

inline bool BgNpcMotionOwner(uint32_t tag,bool inParty,bool conscious) {
    return (tag&0xFFFFFF00u)==0x42474E00u && !inParty && conscious;
}
enum class DoorContextAction { Open, Close, Search, PickLock, Travel, Disarm };
constexpr size_t DoorContextActionCount=6;
struct DoorSecurity {
	bool locked=false, trapped=false, found=false, pickable=true;
	int lockDc=10, searchDc=10, disarmDc=10;
};
inline void ValidateDoorSecurity(const DoorSecurity &s) {
	if (s.lockDc<0 || s.lockDc>60 || s.searchDc<0 || s.searchDc>60 || s.disarmDc<0 || s.disarmDc>60
		|| (s.found && !s.trapped))
		throw std::invalid_argument("invalid door security");
}
// Closed doors can always be inspected. A found trap adds Disable Device; an
// open door offers travel only when a scene route belongs to its marker.
inline std::vector<DoorContextAction> DoorContextActions(bool open,const DoorSecurity &s,bool travel=true,bool disarm=true) {
	if (open) {
		if (travel) return {DoorContextAction::Close,DoorContextAction::Travel};
		return {DoorContextAction::Close};
	}
	std::vector<DoorContextAction> result;
	if (!s.locked) result.push_back(DoorContextAction::Open);
	if (s.locked) result.push_back(DoorContextAction::Search);
	if (disarm && s.trapped && s.found) result.push_back(DoorContextAction::Disarm);
	if (s.locked) result.push_back(DoorContextAction::PickLock);
	return result;
}
inline DoorSecurity SearchObjectTrap(DoorSecurity s,bool success,bool canFindDifficultTraps) {
    if (success && s.trapped && (s.searchDc<=20 || canFindDifficultTraps)) s.found=true;
    return s;
}
// 3.5 Disable Device: failing by five or more springs the trap.
inline bool DoorDisarmSpringsTrap(int deltaFromDc) { return deltaFromDc<=-5; }
// Opening a still armed door springs it once, whether or not it was found.
inline DoorSecurity DoorTrapSprung(DoorSecurity s) { s.trapped=false; s.found=false; return s; }
struct DoorSkillCandidate { uint64_t actor; int bonus; double distance; bool eligible; };
inline uint64_t BestDoorSkillCandidate(const std::vector<DoorSkillCandidate> &candidates) {
	uint64_t chosen=0; int bonus=(std::numeric_limits<int>::min)(); double distance=0;
	for (const auto &c:candidates) if (c.actor && c.eligible && std::isfinite(c.distance)
		&& (!chosen || c.bonus>bonus || (c.bonus==bonus && c.distance<distance))) {
		chosen=c.actor; bonus=c.bonus; distance=c.distance;
	}
	return chosen; // class never participates; negative effective bonuses are valid
}
class DoorContextHover {
public:
	void Reveal(const std::string &id,uint32_t now) {Update(id,now);mStarted=now-450;}
	void Update(const std::string &id,uint32_t now) {
		if (!id.empty()) { if (id!=mId) {mId=id;mStarted=now;} mLeaving=false; }
		else if (!mId.empty()) {if (!mLeaving) {mLeaving=true;mLeft=now;} else if (uint32_t(now-mLeft)>350) Clear();}
	}
	void Clear() {mId.clear();}
	const std::string &Target() const {return mId;}
	bool Active(uint32_t now) const {return !mId.empty() && (!mLeaving || uint32_t(now-mLeft)<=350);}
	bool Visible(uint32_t now) const {return Active(now) && uint32_t(now-mStarted)>=450;}
	float Alpha(uint32_t now) const {
		return Visible(now) ? float((std::min)(1.0,double(uint32_t(now-mStarted)-450)/150.0))*0.68f : 0.0f;
	}
private:
	std::string mId; uint32_t mStarted=0,mLeft=0; bool mLeaving=false;
};
struct DoorContextDefinition { std::array<std::string,DoorContextActionCount> textures; DoorSecurity security; };

// Isolated source areas share one native map. Moving selected actors keeps the
// native party, object IDs, inventory, dispatchers and normal save format intact.
struct OriginalSceneArea {
	int id, nativeMap, x,y,width,height,sourceGround=0,shiftX=0,shiftY=0;
};
inline void ValidateOriginalSceneArea(const OriginalSceneArea &a) {
	if (a.id<=0 || a.nativeMap<=0 || a.x<0 || a.y<0 || a.width<=0 || a.height<=0
		|| a.width>66*256 || a.height>71*256 || a.x>66*256-a.width || a.y>71*256-a.height || a.sourceGround<0
		|| a.shiftX%256 || a.shiftY%256)
		throw std::invalid_argument("invalid isolated scene area");
}
inline bool IsReviewedCandlekeepScene(const OriginalSceneArea &a) {
    if (a.nativeMap!=5136) return false;
    if (a.id==5136 || a.id==5190 || a.id==5191) return true;
    return (a.id==5192 && a.x==2968 && a.y==8818 && a.width==640 && a.height==512
        && a.sourceGround==903 && a.shiftX==-5120 && a.shiftY==0)
        || (a.id==5194 && a.x==2968 && a.y==12402 && a.width==640 && a.height==512
        && a.sourceGround==905 && a.shiftX==-5120 && a.shiftY==3584)
        || (a.id==5195 && a.x==13208 && a.y==8882 && a.width==640 && a.height==384
        && a.sourceGround==906 && a.shiftX==5120 && a.shiftY==0)
        || (a.id==5196 && a.x==13144 && a.y==10610 && a.width==768 && a.height==512
        && a.sourceGround==907 && a.shiftX==5120 && a.shiftY==1792)
        || (a.id==5193 && a.x==2840 && a.y==10514 && a.width==896 && a.height==704
        && a.sourceGround==904 && a.shiftX==-5120 && a.shiftY==1792)
        || (a.id==5197 && a.x==12632 && a.y==11954 && a.width==1792 && a.height==1408
        && a.sourceGround==908 && a.shiftX==5120 && a.shiftY==3584)
        || (a.id==5198 && a.x==10968 && a.y==1778 && a.width==5120 && a.height==3840
        && a.sourceGround==909 && a.shiftX==5120 && a.shiftY==-5376);
}
inline bool SceneContains(const OriginalSceneArea &a,double x,double y) {
	return x>=a.x && y>=a.y && x<a.x+a.width && y<a.y+a.height;
}
inline OriginalDoorUsePoint SceneCanvasPoint(double worldX,double worldY) {
	constexpr double unit=28.2842712474619;
	return {(worldY-worldX)/unit*20+8428,(worldX+worldY)/unit*14-4366};
}
struct OriginalSceneRoute {int source,destination; OriginalDoorUsePoint arrival;};
inline bool SameDoorSelection(std::vector<uint64_t> a,std::vector<uint64_t> b) {
	std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());return !a.empty() && a==b;
}

struct SceneArrivalActor {uint64_t id;double radius;};
struct SceneArrivalSpot {uint64_t id;double radius;OriginalDoorUsePoint point;};
// Shared by native movement and standalone tests. The caller supplies the real
// native collision query. On failure, even the output plan remains untouched.
template<class CanStand> bool PlanSceneArrival(const std::vector<SceneArrivalActor> &actors,
	OriginalDoorUsePoint target,CanStand canStand,std::vector<SceneArrivalSpot> &output) {
	if (actors.empty() || actors.size()>8 || !std::isfinite(target.x) || !std::isfinite(target.y)) return false;
	std::vector<SceneArrivalSpot> plan;
	for (const auto &actor:actors) {
		if (!actor.id || !std::isfinite(actor.radius) || actor.radius<0) return false;
		for (const auto &placed:plan) if (placed.id==actor.id) return false;
		bool found=false;
		for (int ring=0;ring<19 && !found;++ring) for (int dx=-ring;dx<=ring && !found;++dx) for (int dy=-ring;dy<=ring && !found;++dy) {
			if (ring && std::abs(dx)!=ring && std::abs(dy)!=ring) continue;
			OriginalDoorUsePoint point{target.x+dx*28.2842712474619/3,target.y+dy*28.2842712474619/3};
			bool overlap=false;
			for (const auto &placed:plan) if (OriginalDoorUseDistance(point,placed.point)<actor.radius+placed.radius) {overlap=true;break;}
			if (!overlap && canStand(actor,point)) {plan.push_back({actor.id,actor.radius,point});found=true;}
		}
		if (!found) return false;
	}
	output=std::move(plan);return true;
}
