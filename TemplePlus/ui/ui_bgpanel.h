#pragma once

#include <vector>
#include "bgpanelmodel.h"

struct TigMsg;

/*
	bg2toee panel: one modal full-screen window drawn from a list of elements
	that the mod's Python passes (game.bg_panel_show). It carries the BG world
	map, the camp (rest) window and the inn rooms. Clicks, Esc/right click
	(cancel) and Enter (accept) go back to Python through
	templeplus.savehook.bg_panel_event(event, element_id); Python answers by
	showing a new element list or hiding the panel.
*/
namespace bgpanel {

void Show(std::vector<Element> elements);
void Hide();
bool IsVisible();
bool HandleModalInput(const TigMsg &msg);

// On maps where the mod runs rest, ToEE's sleep status reads "safe" so its
// rest button and hotkey reach the camp hook (they skipped it at status 2).
void SetRestTakeover(bool enabled);
bool RestTakeover();

}
