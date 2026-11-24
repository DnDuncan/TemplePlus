#pragma once

#include <string>
#include <vector>

/*
	BG journal window for the bg2toee mod: a scrollable list of dated entries
	(BG AddJournalEntry texts), opened from Python (game.bg_journal_show) and
	by the J key (templeplus.savehook.journal_key). Text is drawn with the
	game's own Priory font from the cp1251 bytes Python passes, exactly like
	dialog text, so Cyrillic renders as in the dialog window.
*/
namespace bgjournal {

struct Entry {
	std::string heading;   // e.g. the date line
	std::string text;
};

void Show(const std::string &title, const std::vector<Entry> &entries);
void Hide();
bool IsVisible();

}
