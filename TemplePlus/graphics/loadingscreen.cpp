#include "stdafx.h"

#include <infrastructure/mesparser.h>

#include "loadingscreen.h"
#include "imgfile.h"
#include "rectangle.h"

#include <config/config.h>
#include <ui/ui_render.h>
#include <tig/tig_font.h>
#include <graphics/shaperenderer2d.h>
#include <graphics/device.h>
#include <graphics/textures.h>

using namespace gfx;

struct LoadingScreen::Impl {

	explicit Impl(RenderingDevice& device, ShapeRenderer2d &shapeRenderer) 
		: device(device), shapeRenderer(shapeRenderer) {
	}

	int barWidth = 512;
	int barHeight = 18;
	int barBorderSize = 1;

	RenderingDevice& device;
	ShapeRenderer2d& shapeRenderer;
	float progress = 0;
	std::string message;
	std::unique_ptr<CombinedImgFile> imageFile;
	TextureRef campaignImage;
	float campaignX = 0, campaignY = 0, campaignWidth = 0, campaignHeight = 0;

	UiRectangle barBorder;
	UiRectangle barUnfilled;
	UiRectangle barFilled;

	bool messagesLoaded = false;
	MesFile::Content messages;

	void Layout();
	void UpdateFilledBarWidth();
};

void LoadingScreen::Impl::Layout() {
	
	auto centerX = device.GetCurrentCamera().GetScreenWidth() / 2.0f;
	auto centerY = device.GetCurrentCamera().GetScreenHeight() / 2.0f;

	int barY;
	if (campaignImage) {
		auto size = campaignImage->GetSize();
		auto scale = (std::min)(centerX * 1.8f / size.width, centerY * 1.6f / size.height);
		campaignWidth = size.width * scale;
		campaignHeight = size.height * scale;
		campaignX = centerX - campaignWidth / 2;
		campaignY = centerY - campaignHeight / 2 - 14;
		barWidth = (std::min)(512, (int)campaignWidth);
		barY = (int)(campaignY + campaignHeight + 12);
	} else {
		auto imgX = (int)(centerX - imageFile->GetWidth() / 2.0f);
		auto imgY = (int)(centerY - imageFile->GetHeight() / 2.0f);
		imageFile->SetX(imgX);
		imageFile->SetY(imgY);
		barY = (int)(imgY + 20 + imageFile->GetHeight());
	}
	auto barX = (int)(centerX - barWidth / 2.0f);

	// Set up the border
	barBorder.SetX(barX);
	barBorder.SetY(barY);
	barBorder.SetWidth(barWidth);
	barBorder.SetHeight(barHeight);

	// Set up the background
	barUnfilled.SetX(barX + barBorderSize);
	barUnfilled.SetY(barY + barBorderSize);
	barUnfilled.SetWidth(barWidth - barBorderSize * 2);
	barUnfilled.SetHeight(barHeight - barBorderSize * 2);

	// Set up the filling (width remains unset)
	barFilled.SetX(barX + barBorderSize);
	barFilled.SetY(barY + barBorderSize);
	barFilled.SetHeight(barHeight - barBorderSize * 2);
	UpdateFilledBarWidth();
}

void LoadingScreen::Impl::UpdateFilledBarWidth() {
	auto fullWidth = barWidth - barBorderSize * 2;
	barFilled.SetWidth((int)(fullWidth * std::min(1.0f, progress)));
}

LoadingScreen::LoadingScreen(RenderingDevice &device, ShapeRenderer2d &shapeRenderer) 
	: mImpl(std::make_unique<Impl>(device, shapeRenderer)) {
	if (_stricmp(config.defaultModule.c_str(), "BG2ToEE") == 0) {
		// BG2ToEE_core is mounted by TIG before game systems are initialized.
		auto image = device.GetTextures().Resolve("art/splash/bg2toee-campaign.png", false);
		if (image && image->IsValid()) {
			mImpl->campaignImage = std::move(image);
			mImpl->Layout();
		}
	}
	if (!mImpl->campaignImage) SetImage("art\\splash\\legal0322.img");

	mImpl->barBorder.SetColor(0xFF808080);
	mImpl->barUnfilled.SetColor(0xFF1C324E);
	mImpl->barFilled.SetColor(0xFF1AC3FF);
}

LoadingScreen::~LoadingScreen() {
}

void LoadingScreen::SetProgress(float progress) {
	mImpl->progress = progress;

	mImpl->UpdateFilledBarWidth();
}

float LoadingScreen::GetProgress() const {
	return mImpl->progress;
}

void LoadingScreen::SetMessageId(uint32_t messageId) {
	if (!mImpl->messagesLoaded) {
		mImpl->messages = MesFile::ParseFile("mes\\loadscreen.mes");
		mImpl->messagesLoaded = true;
	}

	auto it = mImpl->messages.find(messageId);
	if (it == mImpl->messages.end()) {
		mImpl->message = fmt::format("Unknown Message ID: {}", messageId);
	} else {
		mImpl->message = it->second;
	}
}

void LoadingScreen::SetMessage(const std::string& message) {
	mImpl->message = message;
}

void LoadingScreen::SetImage(const std::string& imagePath) {
	mImpl->imageFile = std::make_unique<CombinedImgFile>(imagePath);
	mImpl->Layout();
}

void LoadingScreen::Render() {

	if (!mImpl->campaignImage && !mImpl->imageFile) {
		return;
	}
	
	mImpl->device.BeginFrame();
	if (mImpl->campaignImage) {
		mImpl->shapeRenderer.DrawRectangle(mImpl->campaignX, mImpl->campaignY,
			mImpl->campaignWidth, mImpl->campaignHeight, *mImpl->campaignImage, 0xFFFFFFFF);
	} else {
		mImpl->imageFile->Render();
	}
	mImpl->barBorder.Render();
	mImpl->barUnfilled.Render();
	mImpl->barFilled.Render();

	if (!mImpl->message.empty()) {
		UiRenderer::PushFont(PredefinedFont::ARIAL_10);

		ColorRect textColor(0xFFFFFFFF);
		TigTextStyle style;
		style.flags = 0;
		style.kerning = 1;
		style.tracking = 3;
		style.textColor = &textColor;

		TigRect rect{0, 0, 0, 0};
		rect.x = mImpl->barBorder.GetX();
		rect.y = mImpl->barBorder.GetY() + mImpl->barHeight + 5;

		UiRenderer::RenderText(mImpl->message, rect, style);

		UiRenderer::PopFont();
	}
	mImpl->device.Present();

}
