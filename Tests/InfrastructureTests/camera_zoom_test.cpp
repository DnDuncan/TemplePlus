#include "stdafx.h"
#include <graphics/camera.h>
#include "../../TemplePlus/worldviewzoom.h"
#include <limits>

static XMFLOAT2 Project(const XMFLOAT4X4 &matrix, XMFLOAT3 point, float w, float h) {
	auto p = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&point), DirectX::XMLoadFloat4x4(&matrix));
	return { (DirectX::XMVectorGetX(p) + 1) * w / 2, (1 - DirectX::XMVectorGetY(p)) * h / 2 };
}

TEST(CameraZoom, WheelStepsClampAndAccumulateHighResolutionInput) {
	WorldViewZoom zoom;
	EXPECT_DOUBLE_EQ(1, zoom.Scale());
	EXPECT_FALSE(zoom.Wheel(60));
	EXPECT_TRUE(zoom.Wheel(60));
	zoom.Wheel(5 * 120);
	EXPECT_DOUBLE_EQ(2, zoom.Scale());
	EXPECT_TRUE(zoom.Wheel(120));
	EXPECT_DOUBLE_EQ(2.3, zoom.Scale());
	EXPECT_FALSE(zoom.Wheel(12000));
	zoom.Wheel(-13 * 120);
	EXPECT_DOUBLE_EQ(0.5, zoom.Scale());
	zoom.Wheel((std::numeric_limits<int>::min)());
	EXPECT_DOUBLE_EQ(0.5, zoom.Scale());
	zoom.Reset();
	EXPECT_DOUBLE_EQ(1, zoom.Scale());
	EXPECT_THROW(zoom.Set(0), std::invalid_argument);
	EXPECT_THROW(zoom.Set(2.31), std::invalid_argument);
	EXPECT_THROW(zoom.Set(std::numeric_limits<double>::infinity()), std::invalid_argument);
	zoom.Set(2.3);
	EXPECT_DOUBLE_EQ(2.3, zoom.Scale());
	zoom.Wheel(-120);
	EXPECT_DOUBLE_EQ(2, zoom.Scale());
	zoom.Set(2.14);
	EXPECT_DOUBLE_EQ(2, zoom.Scale());
	zoom.Set(2.15);
	EXPECT_DOUBLE_EQ(2.3, zoom.Scale());
}

TEST(CameraZoom, PointerInverseAndVisibleBoundsShareTheSameTransform) {
	WorldViewZoom zoom;
	for (auto scale : { 0.5, 1.0, 2.0, 2.3 }) {
		zoom.Set(scale);
		for (auto width : { 1280.0, 1440.0 }) {
			for (auto height : { 720.0, 960.0 }) {
				for (auto x : { -200.0, 0.0, 512.0, 1800.0 }) {
					auto visible = zoom.ToDisplay(x, 350, width, height);
					auto logical = zoom.ToWorld(visible.x, visible.y, width, height);
					EXPECT_NEAR(x, logical.x, 0.000001);
					EXPECT_NEAR(350, logical.y, 0.000001);
				}
				auto rect = zoom.Visible(width, height);
				EXPECT_NEAR(width / scale, rect.right - rect.left, 0.000001);
				EXPECT_NEAR(height / scale, rect.bottom - rect.top, 0.000001);
			}
		}
	}
}

TEST(CameraZoom, ThreeDimensionalActorsAndTwoDimensionalArtProjectIdentically) {
	gfx::WorldCamera camera;
	camera.SetScreenSize(1280, 720);
	camera.SetTranslation(-500, -400);
	WorldViewZoom zoom;
	for (auto scale : { 0.5, 1.0, 2.0, 2.3 }) {
		zoom.Set(scale);
		for (XMFLOAT3 world : { XMFLOAT3(0,0,0), XMFLOAT3(1000,0,1600), XMFLOAT3(900,120,1300) }) {
			camera.SetPresentationZoom(1);
			auto original = Project(camera.GetViewProj(), world, 1280, 720);
			camera.SetPresentationZoom(static_cast<float>(scale));
			auto actor = Project(camera.GetViewProj(), world, 1280, 720);
			auto art = Project(camera.GetUiProjection(), XMFLOAT3(original.x, original.y, 0), 1280, 720);
			auto expected = zoom.ToDisplay(original.x, original.y, 1280, 720);
			EXPECT_NEAR(actor.x, art.x, 0.002);
			EXPECT_NEAR(actor.y, art.y, 0.002);
			EXPECT_NEAR(actor.x, expected.x, 0.002);
			EXPECT_NEAR(actor.y, expected.y, 0.002);
		}
	}
}

TEST(CameraZoom, LogicalGroundPickingAndSingleAxisResizeRemainStable) {
	gfx::WorldCamera camera;
	camera.SetScreenSize(1280, 720);
	camera.SetTranslation(-500, -400);
	auto original = camera.ScreenToWorld(300, 400);
	camera.SetPresentationZoom(2);
	auto logical = camera.ScreenToWorld(300, 400);
	EXPECT_NEAR(original.x, logical.x, 0.001);
	EXPECT_NEAR(original.z, logical.z, 0.001);
	camera.GetPickRay(300, 400); // Must update dirty matrices even without a prior render.
	camera.SetScreenSize(1440, 720);
	EXPECT_EQ(1440, camera.GetScreenWidth());
	camera.SetScreenSize(1440, 960);
	EXPECT_EQ(960, camera.GetScreenHeight());
}
