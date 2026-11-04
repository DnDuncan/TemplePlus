#include "stdafx.h"
#include "ui_bgrolls.h"

#include <map>
#include <deque>

#include <temple/dll.h>
#include <graphics/device.h>
#include <graphics/shaperenderer2d.h>
#include <util/fixes.h>

#include "tig/tig_startup.h"
#include "ui/ui.h"
#include "ui/ui_render.h"
#include "ui/widgets/widgets.h"
#include "bonus.h"
#include "condition.h"
#include "history.h"
#include "critter.h"
#include "float_line.h"
#include "inventory.h"
#include "obj.h"
#include "objlist.h"
#include "party.h"
#include "traps.h"
#include "description.h"
#include "skill.h"
#include "gamesystems/gamesystems.h"
#include "gamesystems/objects/objsystem.h"
#include "graphics/mapterrain.h"
#include "action_sequence.h"
#include "d20.h"
#include "bgskillrules.h"
#include "bgdicegeometry.h"
#include "../bgcheckstate.h"
#include "ui_bgcheckresources.h"
#include "gamesystems/timeevents.h"
#include "ui/ui_picker.h"
#include "dispatcher.h"
#include "combat.h"
#include "maps.h"
#include "animgoals/anim.h"

namespace bgrolls {
namespace {

// ------------------------------------------------------------- roll capture

struct Captured {
	bool valid = false;
	objHndl obj;
	int skill = 0, natural = 0, bonus = 0, dc = 0;
};
Captured sCaptured;
int sPassive = 0;
bool sAreaRoll = false;
int sPreviewDepth=0;
using GuidanceFn=int(__cdecl)(DispatcherCallbackArgs);
GuidanceFn *sOriginalGuidance=nullptr;
int __cdecl GuidanceSkillHook(DispatcherCallbackArgs args) {
    if (!sPreviewDepth) return sOriginalGuidance(args);
    auto io=dispatch.DispIoCheckIoType10(static_cast<DispIoObjBonus*>(args.dispIO));
    if (args.GetCondArg(4)>0) io->bonOut->AddBonus(args.GetData1(),34,args.GetData2());
    return 0;
}

using HistoryType2Fn = int(__cdecl)(objHndl, objHndl, int, int, int, int, BonusList*);
HistoryType2Fn *sOrgHistoryType2 = nullptr;

// temple.dll 0x10047CF0: the native skill roll records (actor, skill, dice,
// natural d20, DC, bonus list) in the roll history; that is where the natural
// roll is known.
int __cdecl HistoryType2Hook(objHndl obj, objHndl obj2, int skillIdx, int dicePacked, int rollResult, int dc, BonusList *bonlist) {
	sCaptured.valid = true;
	sCaptured.obj = obj;
	sCaptured.skill = skillIdx;
	sCaptured.natural = rollResult;
	sCaptured.bonus = bonlist ? bonlist->GetEffectiveBonusSum() : 0;
	sCaptured.dc = dc;
	return sOrgHistoryType2(obj, obj2, skillIdx, dicePacked, rollResult, dc, bonlist);
}

class HistoryCaptureFix : public TempleFix {
public:
	void apply() override {
		sOriginalGuidance=replaceFunction<GuidanceFn>(0x100DD360,GuidanceSkillHook);
		sOrgHistoryType2 = replaceFunction<int(__cdecl)(objHndl, objHndl, int, int, int, int, BonusList*)>(0x10047CF0, HistoryType2Hook);
	}
} sHistoryCaptureFix;

// ------------------------------------------------------------- roll window

enum class Outcome { Success, Failure, CriticalFailure, CriticalSuccess };

struct RollEvent {
	std::string actor, skill;
	int natural, bonus, dc;
	Outcome outcome;
	uint32_t start = 0;
};

constexpr uint32_t RollMs = 1200, HoldMs = 1800, FadeMs = 400;
constexpr int WndW = 340, WndH = 196;

std::deque<RollEvent> sQueue;


void Text(const std::string &text, int x, int y, int w, XMCOLOR color, PredefinedFont font, bool center) {
	static ColorRect sColor, sShadow(XMCOLOR(0, 0, 0, 0.7f));
	sColor = ColorRect(color);
	TigTextStyle style(&sColor);
	style.kerning = 1;
	style.tracking = 3;
	style.flags = 8 | (center ? 0x10 : 0);
	style.shadowColor = &sShadow;
	TigRect rect(x, y, w, 60);
	UiRenderer::PushFont(font);
	UiRenderer::RenderText(text, rect, style);
	UiRenderer::PopFont();
}

// Icosahedron (d20) drawn with flat-shaded triangles.
using bgdice::V3;using bgdice::IcoV;using bgdice::IcoF;using bgdice::Rotate;

void Triangle(XMFLOAT2 a, XMFLOAT2 b, XMFLOAT2 c, XMCOLOR color) {
	gfx::Vertex2d v[4]{};
	XMFLOAT2 p[4] = { a, b, c, c };
	for (int i = 0; i < 4; i++) {
		v[i].pos = XMFLOAT4(p[i].x, p[i].y, 0, 1);
		v[i].diffuse = color;
	}
	tig->GetShapeRenderer2d().DrawRectangle(v, static_cast<gfx::Texture*>(nullptr));
}

// Draws the die at (cx, cy); returns the screen centre of the face nearest the viewer.
XMFLOAT2 DrawDie(float cx, float cy, float size, float a, float b, float alpha, XMCOLOR base,int *frontNumber=nullptr) {
	struct Face { float depth; XMFLOAT2 p[3]; float light; XMFLOAT2 centre;int number; };
	std::vector<Face> faces;
	int faceIndex=0;
	for (auto &f : IcoF) {
        auto number=bgdice::Labels()[faceIndex++];
		V3 r[3];
		for (int k = 0; k < 3; k++) r[k] = Rotate(IcoV[f[k]], a, b);
		V3 u{ r[1].x - r[0].x, r[1].y - r[0].y, r[1].z - r[0].z }, w{ r[2].x - r[0].x, r[2].y - r[0].y, r[2].z - r[0].z };
		V3 n{ u.y * w.z - u.z * w.y, u.z * w.x - u.x * w.z, u.x * w.y - u.y * w.x };
		auto len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
		auto zc = (r[0].z + r[1].z + r[2].z) / 3;
		if (n.z < 0) continue;   // back face (faces wind outward)
		Face face;
		face.depth = zc;face.number=number;
		for (int k = 0; k < 3; k++) face.p[k] = XMFLOAT2(cx + r[k].x * size, cy - r[k].y * size);
		face.light = 0.45f + 0.55f * fabsf(n.z / len);
		face.centre = XMFLOAT2(cx + (r[0].x + r[1].x + r[2].x) / 3 * size, cy - (r[0].y + r[1].y + r[2].y) / 3 * size);
		faces.push_back(face);
	}
	std::sort(faces.begin(), faces.end(), [](const Face &l, const Face &r) { return l.depth < r.depth; });
	XMFLOAT2 front(cx, cy);
	std::vector<gfx::Line2d> edges;
	XMCOLOR edge(0.05f, 0.04f, 0.02f, alpha);
	for (auto &f : faces) {
		XMCOLOR c(base.r / 255.0f * f.light, base.g / 255.0f * f.light, base.b / 255.0f * f.light, alpha);
		Triangle(f.p[0], f.p[1], f.p[2], c);
		for (int k = 0; k < 3; k++) edges.emplace_back(f.p[k], f.p[(k + 1) % 3], edge);
		front = f.centre;
        if (frontNumber) {
            *frontNumber=f.number;
            if (&f!=&faces.back() && f.light>0.65f)
                Text(std::to_string(f.number),int(f.centre.x)-15,int(f.centre.y)-7,30,
                    XMCOLOR(0.95f,0.92f,0.83f,alpha*f.light),PredefinedFont::PRIORY_12,true);
        }
	}
	tig->GetShapeRenderer2d().DrawLines(edges);
	return front;
}

bool DrawManual();

class RollWindow : public WidgetContainer {
public:
	RollWindow() : WidgetContainer(WndW, WndH) {}
	void Render() override {
		if (!IsVisible()) return;
		if (DrawManual()) return;
		if (sQueue.empty()) return;
		auto now = timeGetTime();
		auto &e = sQueue.front();
		if (!e.start) e.start = now;
		auto t = now - e.start;
		if (t > RollMs + HoldMs + FadeMs) {
			sQueue.pop_front();
			if (sQueue.empty()) Hide();
			return;
		}
		auto alpha = t > RollMs + HoldMs ? 1.0f - (t - RollMs - HoldMs) / float(FadeMs) : 1.0f;
		auto area = GetContentArea();
		auto &shapes = tig->GetShapeRenderer2d();
		shapes.DrawRectangle((float)area.x, (float)area.y, (float)area.width, (float)area.height,
			XMCOLOR(0.06f, 0.05f, 0.03f, 0.86f * alpha));
		shapes.DrawRectangleOutline({ (float)area.x + 1, (float)area.y + 1 },
			{ (float)(area.x + area.width - 2), (float)(area.y + area.height - 2) }, XMCOLOR(0.62f, 0.50f, 0.27f, alpha));

		XMCOLOR gold(0.90f, 0.78f, 0.45f, alpha), white(0.95f, 0.93f, 0.88f, alpha);
		Text(e.actor + ": " + e.skill, area.x, area.y + 8, area.width, gold, PredefinedFont::PRIORY_12, true);
		Text(fmt::format("\xD1\xEB\xEE\xE6\xED\xEE\xF1\xF2\xFC {}", e.dc), area.x, area.y + 26, area.width, white,
			PredefinedFont::PRIORY_12, true);   // "Сложность"

		auto rolling = t < RollMs;
		auto k = rolling ? t / float(RollMs) : 1.0f;
		auto ease = 1 - (1 - k) * (1 - k) * (1 - k);
		auto a = 9.0f * ease + e.natural * 0.37f, b = 6.0f * ease + e.natural * 0.21f;
		auto dieX = area.x + area.width / 2.0f + (rolling ? (1 - ease) * 90.0f * sinf(k * 9) : 0);
		auto dieY = area.y + 96.0f - (rolling ? fabsf(sinf(k * 12)) * 18 * (1 - ease) : 0);
		XMCOLOR body = e.outcome == Outcome::CriticalFailure ? XMCOLOR(0.62f, 0.12f, 0.10f, alpha)
			: XMCOLOR(0.80f, 0.70f, 0.48f, alpha);
		auto face = DrawDie(dieX, dieY, 26.0f, a, b, alpha, body);
		auto shown = rolling ? 1 + (int)((t / 70 + e.natural * 7) % 20) : e.natural;
		Text(std::to_string(shown), (int)face.x - 30, (int)face.y - 13, 60, white, PredefinedFont::ARIAL_BOLD_24, true);

		if (!rolling) {
			auto total = e.natural + e.bonus;
			Text(fmt::format("{} {} {} = {}", e.natural, e.bonus < 0 ? "-" : "+", abs(e.bonus), total), area.x,
				area.y + 134, area.width, white, PredefinedFont::PRIORY_12, true);
			const char *label = "\xD3\xD1\xCF\xC5\xD5";               // УСПЕХ
			XMCOLOR color(0.30f, 0.85f, 0.35f, alpha);
			if (e.outcome == Outcome::Failure) { label = "\xCF\xD0\xCE\xC2\xC0\xCB"; color = XMCOLOR(0.95f, 0.35f, 0.30f, alpha); }
			if (e.outcome == Outcome::CriticalFailure) { label = "\xCA\xD0\xC8\xD2\xC8\xD7\xC5\xD1\xCA\xC8\xC9 \xCF\xD0\xCE\xC2\xC0\xCB"; color = XMCOLOR(1.0f, 0.22f, 0.18f, alpha); }
			if (e.outcome == Outcome::CriticalSuccess) { label = "\xCA\xD0\xC8\xD2\xC8\xD7\xC5\xD1\xCA\xC8\xC9 \xD3\xD1\xCF\xC5\xD5"; }
			Text(label, area.x, area.y + 156, area.width, color, PredefinedFont::PRIORY_12, true);
		}
	}
};

RollWindow *sWindow = nullptr;

void Show(RollEvent e) {
	if (!sWindow) {
		sWindow = new RollWindow();   // owned by the UI manager until shutdown
		sWindow->SetCenterHorizontally(true);
		sWindow->SetY(70);
	}
	if (sQueue.size() > 6) sQueue.pop_front();
	sQueue.push_back(std::move(e));
	sWindow->Show();
	sWindow->BringToFront();
}

// ------------------------------------------------------------- BG rules

// The streak belongs to the actual tools, and is saved by native inventory.
// Never store handles across loads or damage the next tool after a swap.

// Thieves' tools by quality: plain 0, masterwork 1, mithril 2.
const std::pair<int, int> ToolProtos[] = { {12640, 2}, {12013, 1}, {12012, 0} };

void RecordOpenLock(objHndl performer, bool criticalFailure) {
	for (auto &tool : ToolProtos) {
		auto item = inventory.FindItemByProtoId(performer, tool.first);
		if (!item) continue;
		auto obj = objSystem->GetObject(item);
		auto saved = static_cast<uint32_t>(obj->GetInt32(obj_f_item_pad_i_6));
		if (saved && (saved & 0xFFFFFF00u) != bgskillrules::ToolStamp) return;
		saved = bgskillrules::RecordToolRoll(saved, criticalFailure);
		obj->SetInt32(obj_f_item_pad_i_6, saved);
		if (!bgskillrules::ToolBreaks(saved, tool.second)) return;
		objects.Destroy(item);
		floatSys.floatMesLine(performer, 1, FloatLineColor::Red,
			"\xC2\xEE\xF0\xEE\xE2\xF1\xEA\xEE\xE9 \xED\xE0\xE1\xEE\xF0 \xF1\xEB\xEE\xEC\xE0\xED!");
		return;
	}
}

bool Enabled();
#include "bgmanualcheck.inc"

bool Enabled() {
	return gameSystems && gameSystems->GetTerrain().HasOriginalDoorInput();
}

struct SearchSession {
	objHndl performer;
	int mapId = 0, total = 0;
	LocAndOffsets center;
	uint32_t until = 0;
	bgskillrules::SearchAttempt attempt;
};
SearchSession sSearch;
objHndl sSearchPickerActor;
int sSearchPickerMap = 0;

bool EligibleTrap(objHndl performer, objHndl handle, LocAndOffsets center) {
	auto obj = objSystem->GetObject(handle);
	if (obj->GetFlags() & (OF_DESTROYED | OF_OFF)) return false;
	auto script = obj->GetScript(obj_f_scripts_idx, 0x27);
	auto trap = traps.GetByObj(handle);
	if (!script.scriptId || (script.unk1 & 4) || !trap) return false;
	if (trap->searchDC > 20 && !d20Sys.d20Query(performer, DK_QUE_Critter_Can_Find_Traps)) return false;
	auto a = center.ToInches2D(), b = obj->GetLocationFull().ToInches2D();
	// ListRadius includes object radii. The template checks the anchor exactly.
	if (!bgskillrules::InSearchArea(a.x, a.y, b.x, b.y)) return false;
	return gameSystems->GetTerrain().SceneAreaForActor(performer.handle)
		== gameSystems->GetTerrain().SceneAreaForActor(handle.handle);
}

objHndl BestSearcher(objHndl fallback) {
	PreviewScope preview;
	std::vector<DoorSkillCandidate> candidates;
	auto area = gameSystems->GetTerrain().SceneAreaForActor(fallback.handle);
	for (uint32_t i=0; i<party.CurrentlySelectedNum(); ++i) {
		auto actor = party.GetCurrentlySelected(i);
		if (!actor || critterSys.IsDeadOrUnconscious(actor)
			|| gameSystems->GetTerrain().SceneAreaForActor(actor.handle) != area) continue;
		auto swap = d20Sys.D20QueryPython(actor, "Skill Swap", skill_search);
		auto skill = swap > 0 && swap <= 42 ? static_cast<SkillEnum>(swap-1) : skill_search;
		auto props = skillSys.skillPropsTable[skill];
		auto eligible = skillSys.IsEnabled(skill) && (props.unskilledUseAllow || critterSys.SkillBaseGet(actor,skill)>0);
		if (props.stat == stat_intelligence && d20Sys.d20Query(actor,DK_QUE_CannotUseIntSkill)) eligible=false;
		auto bonus = dispatch.dispatch1ESkillLevel(actor,skill,nullptr,objHndl::null,SRF_Trap_Search);
		candidates.push_back({actor.handle,bonus,0,eligible});
	}
	return objHndl{BestDoorSkillCandidate(candidates)};
}

void __cdecl SearchPicked(const PickerResult &result, void*) {
	auto performer = sSearchPickerActor;
	auto mapId = sSearchPickerMap;
	auto flags = result.flags;
	auto center = result.location;
	sSearchPickerActor = objHndl::null;
	uiPicker.FreeCurrentPicker();
	if ((flags & PRF_CANCELLED) || !(flags & PRF_HAS_LOCATION)
		|| !Enabled() || maps.GetCurrentMapId() != mapId || combatSys.isCombatActive()
		|| !objSystem->IsValidHandle(performer) || !party.IsInParty(performer)
		|| critterSys.IsDeadOrUnconscious(performer)) return;
	auto a = objects.GetLocationFull(performer).ToInches2D(), b = center.ToInches2D();
	if (!bgskillrules::InSearchArea(a.x,a.y,b.x,b.y)) return;
    auto &terrain=gameSystems->GetTerrain();
    ObjList list;list.ListRadius(center,bgskillrules::SearchRadius,OLC_PORTAL|OLC_CONTAINER);
    int dc=terrain.SearchDoorTraps(performer,center,bgskillrules::SearchRadius,0,false);
    for (auto handle:list) if (EligibleTrap(performer,handle,center)) {
        auto value=traps.GetByObj(handle)->searchDC;dc=dc ? (std::min)(dc,value):value;
    }
    if (!dc) dc=10;
    BeginCheck(performer,skill_search,dc,SRF_Trap_Search,"20 ft / 6 s",
        [performer,center]() {
            auto a=objects.GetLocationFull(performer).ToInches2D(),b=center.ToInches2D();
            return bgskillrules::InSearchArea(a.x,a.y,b.x,b.y);
        },
        [performer,center](int natural,int total,bool success) {
            int found=gameSystems->GetTerrain().SearchDoorTraps(performer,center,bgskillrules::SearchRadius,total,true);
            ObjList targets;targets.ListRadius(center,bgskillrules::SearchRadius,OLC_PORTAL|OLC_CONTAINER);
            for (auto handle:targets) if (EligibleTrap(performer,handle,center) && total>=traps.GetByObj(handle)->searchDC) {
                auto obj=objSystem->GetObject(handle);auto script=obj->GetScript(obj_f_scripts_idx,0x27);
                script.unk1|=4;obj->SetScript(obj_f_scripts_idx,0x27,script);++found;
            }
            floatSys.floatMesLine(performer,1,found ? FloatLineColor::Green:FloatLineColor::White,
                found ? "Trap detected":"No traps found");
            logger->info("BG Search result: actor={} natural={} total={} found={}",performer.handle,natural,total,found);
        },bgskillrules::SearchDuration,[center]() {
            gameSystems->GetTerrain().AddSearchTemplate(center,bgskillrules::SearchRadius,timeGetTime()+bgskillrules::SearchDuration);
        });
}

// Search also runs automatically (moving near traps, secret doors): only the
// one made by the Search action of the current action sequence is shown.
bool ExplicitSearch(objHndl performer) {
	auto cur = actSeqSys.actSeqCur ? *actSeqSys.actSeqCur : nullptr;
	if (!cur || cur->performer != performer) return false;
	for (auto i = 0; i < cur->d20ActArrayNum && i < 32; i++)
		if (cur->d20ActArray[i].d20ActType == D20A_SEARCH) return true;
	return false;
}

}

PreviewScope::PreviewScope(){++sPreviewDepth;}
PreviewScope::~PreviewScope(){--sPreviewDepth;}
bool IsBusy() {return sManual!=nullptr;}

bool BeginCheck(objHndl actor,SkillEnum skill,int dc,int flags,const std::string &target,
    std::function<bool()> valid,std::function<void(int,int,bool)> commit,uint32_t duration,std::function<void()> started) {
    if (IsBusy() || !Enabled() || combatSys.isCombatActive() || !objSystem->IsValidHandle(actor)
        || !party.IsInParty(actor) || critterSys.IsDeadOrUnconscious(actor)) return false;
    auto swap=d20Sys.D20QueryPython(actor,"Skill Swap",skill);
    if (swap>0 && swap<=42) skill=static_cast<SkillEnum>(swap-1);
    auto session=std::make_unique<ManualCheck>();session->actor=actor;session->skill=skill;
    session->map=maps.GetCurrentMapId();session->flags=flags;session->target=target;
    session->state.dc=dc;session->state.duration=duration;
    {PreviewScope preview;dispatch.dispatch1ESkillLevel(actor,skill,&session->base,objHndl::null,flags);}
    session->resources=ScanCheckResources(actor,skill);session->valid=std::move(valid);
    session->commit=std::move(commit);session->started=std::move(started);
    LockCheckTime();sManual=std::move(session);
    if (!sWindow) sWindow=new RollWindow();
    sWindow->SetCenterHorizontally(false);sWindow->SetCenterVertically(false);sWindow->SetPos(0,0);
    sWindow->SetSize(uiManager->GetCanvasSize());sWindow->Show();sWindow->BringToFront();
    logger->info("BG manual check prepared: actor={} skill={} dc={} options={}",actor.handle,int(skill),dc,sManual->resources.size());
    return true;
}

bool HandleModalInput(const TigMsg &msg) {
    if (!sManual || msg.type==TigMsgType::UPDATE_TIME || msg.type==TigMsgType::EXIT) return false;
    if (!ManualValid()) {CloseManual();return true;}
    if (msg.type==TigMsgType::MOUSE) {
        auto area=ManualArea();int x=int(msg.arg1)-area.x,y=int(msg.arg2)-area.y;
        if ((msg.arg4&MSF_SCROLLWHEEL_CHANGE) && sManual->state.phase==bgchecks::Phase::Preparing) {
            if (int(msg.arg3)>0 && sManual->scroll) --sManual->scroll;
            if (int(msg.arg3)<0 && sManual->scroll+ResourceRows<sManual->resources.size()) ++sManual->scroll;
        }
        if (msg.arg4&MSF_LMB_RELEASED) {
            if (x>=165 && x<435 && y>=478 && y<516) {
                if (sManual->state.phase==bgchecks::Phase::Result) FinishManual();else RollManual();
            } else if (sManual->state.phase==bgchecks::Phase::Preparing && x>=20 && x<ManualW-20
                && y>=ResourceTop && y<ResourceTop+ResourceRows*ResourceStep) {
                auto index=sManual->scroll+(y-ResourceTop)/ResourceStep;
                if (index<sManual->resources.size()) {
                    auto &r=sManual->resources[index];auto choose=!r.selected;
                    if (choose && ResourceGain(r)<=0) return true;
                    // One source of each type: never charge two suppressed spells.
                    if (choose) for (auto &other:sManual->resources) if (other.type==r.type) other.selected=false;
                    r.selected=choose;
                }
            }
        }
    } else if (msg.type==TigMsgType::CHAR && msg.arg1==VK_ESCAPE) {
        if (sManual->state.Cancel()) CloseManual();
        else if (sManual->state.phase==bgchecks::Phase::Result) FinishManual();
    }
    // No save/load hotkey, party selection or stale widget click passes through.
    return true;
}

bool BeginAreaSearch(objHndl performer) {
	if (!Enabled()) return false;
	if (IsBusy() || combatSys.isCombatActive() || uiPicker.PickerActiveCheck()) return true;
	performer = BestSearcher(performer);
	if (!performer) return true;
	PickerArgs picker;
	picker.flagsTarget = static_cast<UiPickerFlagsTarget>(Radius | Range | LosNotRequired);
	picker.modeTarget = UiPickerType::Area;
	picker.radiusTarget = 20;
	picker.range = 20;
	picker.incFlags = UIPI_None;
	picker.caster = performer;
	picker.callback = SearchPicked;
	sSearchPickerActor = performer;
	sSearchPickerMap = maps.GetCurrentMapId();
	if (!uiPicker.ShowPicker(picker,nullptr)) sSearchPickerActor = objHndl::null;
	return true;
}

void AdvanceSearch() {
    if (sManual && !ManualValid()) CloseManual();
}

void Reset() {
    CloseManual();
	sCaptured.valid = false;
	sSearch = {};
	sSearchPickerActor = objHndl::null;
	sQueue.clear();
	if (sWindow) sWindow->Hide();
}

void BeginSkillRoll() {
	sCaptured.valid = false;
}

BOOL EndSkillRoll(objHndl performer, SkillEnum skill, int dc, int *delta, int flags, BOOL success) {
	auto rec = sCaptured;
	sCaptured.valid = false;
	if (!Enabled() || sPassive || !rec.valid || !performer || !party.IsInParty(performer)
		|| rec.obj != performer || rec.skill != skill || rec.natural < 1 || rec.natural > 20) return success;
	if (skill == skill_search && ((flags & SRF_Hide_Failed_Search) || (!sAreaRoll && !ExplicitSearch(performer)))) return success;
	auto natural = rec.natural;
	// Explicit house rule requested for these two skills: a natural 1 is
	// critical failure even when a large skill bonus would meet the DC.
	if (natural == 1 && (skill == skill_open_lock || skill == skill_disable_device)) {
		success = FALSE;
		if (delta) *delta = (std::min)(*delta,-1);
	}
	auto outcome = success ? Outcome::Success : Outcome::Failure;
	if (natural == 20 && success) outcome = Outcome::CriticalSuccess;
	if (skill == skill_disable_device && !success) {
		// Only a natural 1 springs the trap; the native rule is "fail by 5 or more".
		if (natural == 1) outcome = Outcome::CriticalFailure;
		if (delta) *delta = bgskillrules::DisableFailureDelta(natural,*delta);   // temple.dll 0x10051606: springs when delta <= -4
	} else if (skill == skill_open_lock) {
		auto criticalFailure = !success && natural == 1;
		if (criticalFailure) outcome = Outcome::CriticalFailure;
		RecordOpenLock(performer,criticalFailure);
	}
	RollEvent e;
	auto actor = description.getDisplayName(performer);
	e.actor = actor ? actor : "";
	auto name = skillSys.GetSkillName(skill);
	e.skill = name ? name : "";
	e.natural = natural;
	e.bonus = rec.bonus;
	e.dc = rec.dc ? rec.dc : dc;
	e.outcome = outcome;
	Show(e);
	return success;
}

PassiveScope::PassiveScope() { sPassive++; }
PassiveScope::~PassiveScope() { sPassive--; }

}
