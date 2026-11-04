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
struct V3 { float x, y, z; };
const float Phi = 1.6180339887f;
const V3 IcoV[12] = {{-1,Phi,0},{1,Phi,0},{-1,-Phi,0},{1,-Phi,0},{0,-1,Phi},{0,1,Phi},{0,-1,-Phi},{0,1,-Phi},
	{Phi,0,-1},{Phi,0,1},{-Phi,0,-1},{-Phi,0,1}};
const int IcoF[20][3] = {{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
	{3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};

V3 Rotate(V3 v, float a, float b) {
	auto ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
	V3 r{ v.x * ca + v.z * sa, v.y, -v.x * sa + v.z * ca };
	return { r.x, r.y * cb - r.z * sb, r.y * sb + r.z * cb };
}

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
XMFLOAT2 DrawDie(float cx, float cy, float size, float a, float b, float alpha, XMCOLOR base) {
	struct Face { float depth; XMFLOAT2 p[3]; float light; XMFLOAT2 centre; };
	std::vector<Face> faces;
	for (auto &f : IcoF) {
		V3 r[3];
		for (int k = 0; k < 3; k++) r[k] = Rotate(IcoV[f[k]], a, b);
		V3 u{ r[1].x - r[0].x, r[1].y - r[0].y, r[1].z - r[0].z }, w{ r[2].x - r[0].x, r[2].y - r[0].y, r[2].z - r[0].z };
		V3 n{ u.y * w.z - u.z * w.y, u.z * w.x - u.x * w.z, u.x * w.y - u.y * w.x };
		auto len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
		auto zc = (r[0].z + r[1].z + r[2].z) / 3;
		if (n.z < 0) continue;   // back face (faces wind outward)
		Face face;
		face.depth = zc;
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
	}
	tig->GetShapeRenderer2d().DrawLines(edges);
	return front;
}

class RollWindow : public WidgetContainer {
public:
	RollWindow() : WidgetContainer(WndW, WndH) {}
	void Render() override {
		if (!IsVisible() || sQueue.empty()) return;
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

std::map<uint64_t, int> sLockFumbles;   // by object handle (party members stay loaded)

// Thieves' tools by quality: plain 0, masterwork 1, mithril 2.
const std::pair<int, int> ToolProtos[] = { {12640, 2}, {12013, 1}, {12012, 0} };

void OpenLockFumble(objHndl performer) {
	auto &count = sLockFumbles[performer.handle];
	count++;
	for (auto &tool : ToolProtos) {
		auto item = inventory.FindItemByProtoId(performer, tool.first);
		if (!item) continue;
		if (count < 2 + tool.second) return;
		objects.Destroy(item);
		count = 0;
		floatSys.floatMesLine(performer, 1, FloatLineColor::Red,
			"\xC2\xEE\xF0\xEE\xE2\xF1\xEA\xEE\xE9 \xED\xE0\xE1\xEE\xF0 \xF1\xEB\xEE\xEC\xE0\xED!");   // Воровской набор сломан!
		return;
	}
}

void AreaSearch(objHndl performer, int total) {
	auto loc = objects.GetLocationFull(performer);
	gameSystems->GetTerrain().AddSearchTemplate(loc, 360.0f, timeGetTime() + 30000);
	ObjList list;
	list.ListRadius(loc, 360.0f, OLC_PORTAL | OLC_CONTAINER);
	for (auto i = 0; i < list.size(); i++) {
		auto handle = list[i];
		auto obj = objSystem->GetObject(handle);
		auto script = obj->GetScript(obj_f_scripts_idx, 0x27);
		if (!script.scriptId || (script.unk1 & 4)) continue;
		auto trap = traps.GetByObj(handle);
		if (!trap || total < trap->searchDC) continue;
		script.unk1 |= 4;   // the native "spotted" bit: red outline, Disable Device available
		obj->SetScript(obj_f_scripts_idx, 0x27, script);
		floatSys.floatMesLine(handle, 1, FloatLineColor::Red,
			"\xCB\xEE\xE2\xF3\xF8\xEA\xE0 \xEE\xE1\xED\xE0\xF0\xF3\xE6\xE5\xED\xE0!");   // Ловушка обнаружена!
	}
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

void BeginSkillRoll() {
	sCaptured.valid = false;
}

BOOL EndSkillRoll(objHndl performer, SkillEnum skill, int dc, int *delta, int flags, BOOL success) {
	auto rec = sCaptured;
	sCaptured.valid = false;
	if (sPassive || !rec.valid || !performer || !party.IsInParty(performer)) return success;
	if (skill == skill_search && ((flags & SRF_Hide_Failed_Search) || !ExplicitSearch(performer))) return success;
	auto natural = rec.natural;
	auto total = natural + rec.bonus;
	auto outcome = success ? Outcome::Success : Outcome::Failure;
	if (natural == 20 && success) outcome = Outcome::CriticalSuccess;
	if (skill == skill_disable_device && !success) {
		// Only a natural 1 springs the trap; the native rule is "fail by 5 or more".
		if (natural == 1) { outcome = Outcome::CriticalFailure; if (delta) *delta = (std::min)(*delta, -10); }
		else if (delta) *delta = (std::max)(*delta, -3);   // temple.dll 0x10051606: springs when delta <= -4
	} else if (skill == skill_open_lock) {
		if (!success && natural == 1) { outcome = Outcome::CriticalFailure; OpenLockFumble(performer); }
		else sLockFumbles.erase(performer.handle);
	} else if (skill == skill_search) {
		AreaSearch(performer, total);
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
