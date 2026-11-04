#pragma once

#include "common.h"
#include "skill.h"

/*
	bg2toee skill checks (Baldur's Gate 3 style):
	- every skill roll of a party member made by an action (not passive Search
	  for secret doors) is shown in a roll window: rolling d20, DC, bonus,
	  total and Success / Failure / Critical failure;
	- Disable Device: only a natural 1 springs the trap (a plain failure
	  just fails); Open Lock: consecutive natural 1s break the thieves' tools
	  (2 for plain tools, one more per tool quality step);
	- Search covers a 30 ft template around the searcher for 30 seconds: every
	  trap in it whose search DC the roll beats is found.
*/
namespace bgrolls {

// Called by the skill roll replacement around the native roll.
void BeginSkillRoll();
BOOL EndSkillRoll(objHndl performer, SkillEnum skill, int dc, int *delta, int flags, BOOL success);

// Passive checks (secret door scanning) are not shown and get no rules.
struct PassiveScope {
	PassiveScope();
	~PassiveScope();
};

}
