#include "stdafx.h"
#include "ui_bgjournal.h"

#include <graphics/device.h>
#include <graphics/shaperenderer2d.h>

#include "tig/tig_startup.h"
#include "ui/ui.h"
#include "ui/ui_render.h"
#include "ui/widgets/widgets.h"

namespace bgjournal {
namespace {

constexpr int Width = 560;
constexpr int Height = 460;
constexpr int Margin = 16;
constexpr int TitleHeight = 30;
constexpr int ListTop = Margin + TitleHeight;
constexpr int ListHeight = Height - ListTop - 52;
constexpr int EntryGap = 10;

ColorRect &HeadingColor() { static ColorRect c(XMCOLOR(0.86f, 0.74f, 0.42f, 1.0f)); return c; }
ColorRect &TextColor() { static ColorRect c(XMCOLOR(0.92f, 0.90f, 0.84f, 1.0f)); return c; }
ColorRect &ShadowColor() { static ColorRect c(XMCOLOR(0.0f, 0.0f, 0.0f, 0.6f)); return c; }

TigTextStyle Style(ColorRect &color) {
	TigTextStyle style(&color);
	style.leading = 0;
	style.kerning = 1;
	style.tracking = 3;
	style.flags = 8; // drop shadow
	style.shadowColor = &ShadowColor();
	return style;
}

int MeasureHeight(const std::string &text, const TigTextStyle &style, int width) {
	if (text.empty()) return 0;
	UiRenderer::PushFont(PredefinedFont::PRIORY_12);
	auto rect = UiRenderer::MeasureTextSize(text, style, width, 0);
	UiRenderer::PopFont();
	return rect.height;
}

// One block of legacy-font text (cp1251 bytes, wrapped to the widget width).
class TextBlock : public WidgetContainer {
public:
	TextBlock(const std::string &text, ColorRect &color, int width)
		: WidgetContainer(width, 1), mColor(color) {
		SetText(text);
	}
	void SetText(const std::string &text) {
		mText = text;
		SetHeight((std::max)(1, MeasureHeight(mText, Style(mColor), GetWidth())));
	}
	void Render() override {
		if (!IsVisible() || mText.empty()) return;
		auto area = GetContentArea();
		auto style = Style(mColor);
		UiRenderer::PushFont(PredefinedFont::PRIORY_12);
		UiRenderer::RenderText(mText, area, style);
		UiRenderer::PopFont();
	}
private:
	std::string mText;
	ColorRect &mColor;
};

// Window background: dark parchment-like panel with a gold frame.
class Panel : public WidgetContainer {
public:
	Panel() : WidgetContainer(Width, Height) {}
	void Render() override {
		if (!IsVisible()) return;
		auto area = GetContentArea();
		auto &shapes = tig->GetShapeRenderer2d();
		shapes.DrawRectangle((float)area.x, (float)area.y, (float)area.width, (float)area.height, 0xF0141008);
		shapes.DrawRectangleOutline({ (float)area.x + 1, (float)area.y + 1 },
			{ (float)(area.x + area.width - 2), (float)(area.y + area.height - 2) }, XMCOLOR(0xFF8C7442));
		shapes.DrawRectangleOutline({ (float)area.x + 4, (float)area.y + 4 },
			{ (float)(area.x + area.width - 5), (float)(area.y + area.height - 5) }, XMCOLOR(0xFF4A3D22));
		WidgetContainer::Render();
	}
};

Panel *sPanel = nullptr;                 // created on first use, lives until exit
TextBlock *sTitle = nullptr;
WidgetScrollView *sList = nullptr;

void Create() {
	// Never destroyed: the UI manager owns the legacy windows until shutdown.
	sPanel = new Panel();
	sPanel->SetCenterHorizontally(true);
	sPanel->SetCenterVertically(true);
	sPanel->SetMouseMsgHandler([](const TigMouseMsg &) { return true; }); // modal for clicks on it

	auto titleBlock = std::make_unique<TextBlock>("", HeadingColor(), Width - 2 * Margin);
	titleBlock->SetPos(Margin, Margin + 4);
	sTitle = titleBlock.get();
	sPanel->Add(std::move(titleBlock));

	auto list = std::make_unique<WidgetScrollView>(Width - 2 * Margin, ListHeight);
	list->SetPos(Margin, ListTop);
	sList = list.get();
	sPanel->Add(std::move(list));

	auto close = std::make_unique<WidgetButton>();
	close->SetStyle(eastl::string("cancel-button"));
	close->SetClickHandler([]() { Hide(); });
	auto closePtr = close.get();
	sPanel->Add(std::move(close));
	closePtr->SetPos((Width - closePtr->GetWidth()) / 2, Height - Margin - 32);
	sPanel->Hide();
}

}

void Show(const std::string &title, const std::vector<Entry> &entries) {
	if (!sPanel) Create();
	sTitle->SetText(title);

	sList->Clear();
	auto width = sList->GetInnerWidth();
	int y = 0;
	for (auto &entry : entries) {
		if (!entry.heading.empty()) {
			auto heading = std::make_unique<TextBlock>(entry.heading, HeadingColor(), width);
			heading->SetPos(0, y);
			y += heading->GetHeight() + 2;
			sList->Add(std::move(heading));
		}
		auto text = std::make_unique<TextBlock>(entry.text, TextColor(), width);
		text->SetPos(0, y);
		y += text->GetHeight() + EntryGap;
		sList->Add(std::move(text));
	}
	// The scroll view sizes its range before adding a child: a last spacer
	// makes the final entry reachable.
	auto spacer = std::make_unique<TextBlock>("", TextColor(), width);
	spacer->SetPos(0, y);
	sList->Add(std::move(spacer));
	sList->Add(std::make_unique<TextBlock>("", TextColor(), width));

	sPanel->Show();
	sPanel->BringToFront();
}

void Hide() {
	if (sPanel) sPanel->Hide();
}

bool IsVisible() {
	return sPanel && sPanel->IsVisible();
}

}
