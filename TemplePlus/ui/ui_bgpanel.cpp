#include "stdafx.h"
#include "ui_bgpanel.h"

#include <graphics/device.h>
#include <graphics/shaperenderer2d.h>

#include "tig/tig_startup.h"
#include "tig/tig_msg.h"
#include "ui/ui.h"
#include "ui/ui_render.h"
#include "ui/widgets/widgets.h"
#include "gamesystems/gamesystems.h"
#include "gamesystems/legacysystems.h"
#include "gamesystems/objects/objsystem.h"
#include "python/python_integration_obj.h"

namespace bgpanel {
namespace {

constexpr uint32_t DikEscape = 0x01, DikReturn = 0x1C, DikNumpadEnter = 0x9C;

std::vector<Element> sElements;
int sMouseX = -1, sMouseY = -1;
bool sRestTakeover = false;

XMCOLOR Argb(uint32_t c) {
	return XMCOLOR(((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f, ((c >> 24) & 0xFF) / 255.0f);
}

TigTextStyle TextStyle(ColorRect &color, bool center) {
	static ColorRect sShadow(XMCOLOR(0, 0, 0, 0.7f));
	TigTextStyle style(&color);
	style.kerning = 1;
	style.tracking = 3;
	style.flags = 8 | (center ? 0x10 : 0); // drop shadow, centred
	style.shadowColor = &sShadow;
	return style;
}

void PanelText(const std::string &text, int x, int y, int w, int h, uint32_t argb, bool center) {
	if (text.empty()) return;
	static ColorRect sColor;
	sColor = ColorRect(Argb(argb));
	auto style = TextStyle(sColor, center);
	TigRect rect(x, y, w, h);
	UiRenderer::PushFont(PredefinedFont::PRIORY_12);
	UiRenderer::RenderText(text, rect, style);
	UiRenderer::PopFont();
}

void DrawTexture(const std::string &path, const Element &e) {
	if (path.empty()) return;
	auto texture = tig->GetRenderingDevice().GetTextures().Resolve(path, false);
	if (!texture || !texture->IsValid()) return;
	tig->GetShapeRenderer2d().DrawRectangle((float)e.x, (float)e.y, (float)e.w, (float)e.h, *texture, e.color);
}

void Outline(const Element &e, uint32_t argb) {
	if (!argb) return;
	tig->GetShapeRenderer2d().DrawRectangleOutline({ (float)e.x, (float)e.y },
		{ (float)(e.x + e.w - 1), (float)(e.y + e.h - 1) }, Argb(argb));
}

std::string PortraitPath(uint64_t handle) {
	objHndl obj{ handle };
	if (!handle || !objSystem->IsValidHandle(obj)) return std::string();
	auto id = objSystem->GetObject(obj)->GetInt32(obj_f_critter_portrait);
	return gameSystems->GetPortrait().GetPortraitFileFromId(id, 0);
}

void DrawElement(const Element &e, bool hovered) {
	auto &shapes = tig->GetShapeRenderer2d();
	switch (e.kind) {
	case Kind::Rect:
		if (e.color >> 24) shapes.DrawRectangle((float)e.x, (float)e.y, (float)e.w, (float)e.h, e.color);
		Outline(e, e.border);
		break;
	case Kind::Image:
		DrawTexture(e.path, e);
		Outline(e, hovered && e.click ? 0xFFE8D28A : e.border);
		break;
	case Kind::Portrait:
		DrawTexture(PortraitPath(e.obj), e);
		Outline(e, hovered && e.click ? 0xFFE8D28A : e.border);
		break;
	case Kind::Text:
		PanelText(e.text, e.x, e.y, e.w, e.h, e.color, e.align == 1);
		break;
	case Kind::Button: {
		auto fill = !e.enabled ? 0xD0201C16u : hovered ? 0xF04A3D22u : 0xF02A2216u;
		shapes.DrawRectangle((float)e.x, (float)e.y, (float)e.w, (float)e.h, fill);
		Outline(e, e.enabled ? 0xFF8C7442 : 0xFF4A4438);
		auto textColor = e.enabled ? e.color : 0xFF807868u;
		PanelText(e.text, e.x, e.y + (e.h - 14) / 2, e.w, e.h, textColor, true);
		break;
	}
	}
}

void DrawTooltip() {
	auto index = TooltipAt(sElements, sMouseX, sMouseY);
	if (index < 0) return;
	const auto &text = sElements[index].tip;
	static ColorRect sColor(XMCOLOR(0.95f, 0.93f, 0.88f, 1.0f));
	auto style = TextStyle(sColor, false);
	UiRenderer::PushFont(PredefinedFont::PRIORY_12);
	auto size = UiRenderer::MeasureTextSize(text, style, 320, 0);
	UiRenderer::PopFont();
	auto canvas = uiManager->GetCanvasSize();
	auto box = TooltipBox(sMouseX, sMouseY, size.width + 16, size.height + 12, canvas.width, canvas.height);
	auto &shapes = tig->GetShapeRenderer2d();
	shapes.DrawRectangle((float)box.x, (float)box.y, (float)box.w, (float)box.h, 0xF0141008);
	shapes.DrawRectangleOutline({ (float)box.x, (float)box.y }, { (float)(box.x + box.w - 1), (float)(box.y + box.h - 1) }, XMCOLOR(0xFF8C7442));
	TigRect rect(box.x + 8, box.y + 6, 320, size.height + 4);
	UiRenderer::PushFont(PredefinedFont::PRIORY_12);
	UiRenderer::RenderText(text, rect, style);
	UiRenderer::PopFont();
}

class PanelWindow : public WidgetContainer {
public:
	PanelWindow() : WidgetContainer(800, 600) {}
	void Render() override {
		if (!IsVisible()) return;
		auto hit = HitTest(sElements, sMouseX, sMouseY);
		for (int i = 0; i < int(sElements.size()); ++i) {
			if (!sElements[i].follow) { DrawElement(sElements[i], i == hit); continue; }
			auto moved = sElements[i];
			moved.x += sMouseX;
			moved.y += sMouseY;
			DrawElement(moved, false);
		}
		DrawTooltip();
	}
};

PanelWindow *sWindow = nullptr;   // created on first use, lives until exit

void Notify(const char *event, const std::string &id) {
	auto args = Py_BuildValue("(ss)", event, id.c_str());
	auto result = pythonObjIntegration.ExecuteScript("templeplus.savehook", "bg_panel_event", args);
	Py_DECREF(args);
	Py_XDECREF(result);
}

}

void Show(std::vector<Element> elements) {
	sElements = std::move(elements);
	if (!sWindow) sWindow = new PanelWindow();
	sWindow->SetCenterHorizontally(false);
	sWindow->SetCenterVertically(false);
	sWindow->SetPos(0, 0);
	sWindow->SetSize(uiManager->GetCanvasSize());
	sWindow->Show();
	sWindow->BringToFront();
}

void Hide() {
	sElements.clear();
	if (sWindow) sWindow->Hide();
}

bool IsVisible() {
	return sWindow && sWindow->IsVisible();
}

void SetRestTakeover(bool enabled) {
	sRestTakeover = enabled;
}

bool RestTakeover() {
	return sRestTakeover;
}

bool HandleModalInput(const TigMsg &msg) {
	if (!IsVisible() || msg.type == TigMsgType::UPDATE_TIME) return false;
	if (msg.type == TigMsgType::EXIT) { Hide(); return false; }
	if (msg.type == TigMsgType::MOUSE) {
		sMouseX = int(msg.arg1);
		sMouseY = int(msg.arg2);
		if (msg.arg4 & MSF_LMB_DOWN) {
			auto index = HitTest(sElements, sMouseX, sMouseY);
			if (index >= 0) {
				auto id = sElements[index].id;
				Notify("press", id);
			}
		}
		if (msg.arg4 & MSF_LMB_RELEASED) {
			auto index = HitTest(sElements, sMouseX, sMouseY);
			auto id = index >= 0 ? sElements[index].id : std::string();   // the callback may replace the list
			Notify("release", id);
			if (index >= 0 && IsVisible()) Notify("click", id);
		} else if (msg.arg4 & MSF_RMB_RELEASED) {
			Notify("cancel", std::string());
		}
		return true;
	}
	if (msg.type == TigMsgType::KEYSTATECHANGE) {
		if (!msg.arg2) {   // key released
			if (msg.arg1 == DikEscape) Notify("cancel", std::string());
			else if (msg.arg1 == DikReturn || msg.arg1 == DikNumpadEnter) Notify("accept", std::string());
		}
		return true;
	}
	return msg.type == TigMsgType::CHAR || msg.type == TigMsgType::KEYDOWN;
}

}
