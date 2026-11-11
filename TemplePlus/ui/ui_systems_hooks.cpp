
#include "stdafx.h"

#include <util/fixes.h>

#include "ui_system.h"
#include "ui_systems.h"
#include "ui_legacysystems.h"
#include "python/python_integration_obj.h"

// bg2toee: templeplus.savehook decides whether a BG window replaces the ToEE
// one (rest_key: camp, worldmap_key: world map). False keeps ToEE behaviour.
static bool BgWindowHook(const char *event, int mode) {
	auto args = Py_BuildValue("(i)", mode);
	auto result = pythonObjIntegration.ExecuteScript("templeplus.savehook", event, args);
	Py_DECREF(args);
	bool handled = result && PyObject_IsTrue(result) == 1;
	Py_XDECREF(result);
	return handled;
}

static class UiSystemHooks : public TempleFix {
public:

	virtual void apply() {

		// gameuilib_reset
		replaceFunction<void()>(0x10115270, []() {
			uiSystems->Reset();
		});

		// call_ui_resize_screen
		replaceFunction<void(UiResizeArgs*)>(0x1009a5c0, [](UiResizeArgs *args) {
			uiSystems->ResizeViewport(args->rect2.width, args->rect2.height);
		});
		
		// HookUiOptions();

		// Camp
		replaceFunction<BOOL(int)>(0x1010EF00, [](int hoursToRest){
			return uiSystems->GetCamping().Camp(hoursToRest);
		});

		// Camping window show (rest button and hotkey)
		static void(__cdecl *orgCampShow)() = replaceFunction<void(__cdecl)()>(0x1012F0C0, []() {
			if (BgWindowHook("rest_key", 0)) return;
			orgCampShow();
		});

		// Worldmap show (button, dialog travel)
		static void(__cdecl *orgWorldmapShow)(int) = replaceFunction<void(__cdecl)(int)>(0x1015F140, [](int mode) {
			if (BgWindowHook("worldmap_key", mode)) return;
			orgWorldmapShow(mode);
		});

		// SetHoursToRestUntilHealed
		replaceFunction<void()>(0x1012EC70, []() {
			uiSystems->GetCamping().SetTimeUntilHealed();
		});
		
		/*static BOOL(__cdecl*orgSetViewport)(int) = replaceFunction<BOOL(int)>(0x10115040, [](int viewport)->BOOL {
				logger->trace("SetViewport: {}", viewport);
				logger->trace("Viewport idx before: {}", temple::GetRef<int>(0x10BD3B44));
				auto result = orgSetViewport(viewport);
				logger->trace("Viewport idx after: {}", temple::GetRef<int>(0x10BD3B44));
				return result;
			});*/
	}

private:

	/*void HookUiOptions() {
		// ui_options_hide
		replaceFunction<void()>(0x10117780, []() {
			uiSystems->GetOptions().Hide();
		});
		// ui_options_is_visible
		replaceFunction<BOOL()>(0x101177d0, []() {
			return uiSystems->GetOptions().IsVisible() ? TRUE : FALSE;
		});
		// ui_options_show
		replaceFunction<void(int)>(0x10119d20, [](BOOL fromMainMenu) {
			uiSystems->GetOptions().Show(fromMainMenu == TRUE);
		});
	}*/

} hooks;
