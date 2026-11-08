# Individual party engagement (draft)

Follow-up to engine PR31 / 0.2.141. A shared logical room must not automatically
put every party member into initiative. This package changes recruitment for BG
scenes on map 5136; it does not add several independent encounter schedulers.

## Behavior

- Seed the real initiator before gathering party candidates. Capture an NPC's
  actual party target before native initiative-add hooks can change its focus.
  The initial pair retains native combat-start interruption rules.
- Other party members enter when a hostile participant targets or detects them,
  or when they perform a hostile action/help a participant (the PR31 action path).
  Automatic recruitment uses the existing 42.5-foot activation range, scene
  filtering and native sight/hearing/concealment rules. A room match alone is
  insufficient. Existing members are never expelled or rerolled by recruitment.
- An unrelated, already performing free action completes before automatic or
  native-requested recruitment. Focus/perception is rechecked by the scene pulse.
  This avoids switching a healing callback into another hero's combat turn.
- Hidden perception results are reused for 6000ms per observer/target/signature,
  including failed checks. Render pulses therefore do not repeatedly reroll
  Spot/Listen. Range/scene and direct threats are checked outside the cache.
  Ordinary revealed actors are checked without the hidden-roll cache. The cache
  resets on encounter start/end and context clear during map/save restoration.
- Party startup, rectangle scans, repeated end-turn scans, scene entry and combat
  animation interruption all use the same individual recruitment gate.
- Project NPC.attack(pc) provokes only that PC rather than every PC in the room.
  AI target selection excludes detached PCs until the recruitment pass adds them.
- After a selected free hero joins, selection keeps the remaining free heroes or
  returns to the current party turn (clears during an NPC turn). Ctrl cannot mix
  the current fighter into a free selection. No other actor's turn is advanced.

## Persistence and boundaries

The native initiative list remains authoritative and already saved by ToEE.
No actor GUIDs, NPC ledgers, saved globals, maps, loot, prototypes or save schemas
are replaced. Detection cache handles are transient and never written to saves.
Loading an already-started encounter retains its existing membership; use a save
before combat to test the new recruitment rules.

Separate simultaneous encounters, independent world clocks, actor-local dialogues
and skill-check completion remain outstanding. Primary-target combat interruption
is still native ToEE behavior; this is not a complete BG3 action-commit model.

## Validation / manual checks

Native tests cover individual recruitment, in-flight action deferral, initiating
pair, existing membership, scene/availability rejection, selection after joining,
hidden perception cadence, distinct observers, invalidation/reset and clock wrap.
Seven local Python savehook tests and 403 converter unittest tests passed.
Full native compilation and gameplay are not established by these local tests.
No local C++ compiler was found; the native tests run in Windows CI.

After a successful CI build and manual engine setup, start from before a fight:
1. Start with A while hidden B shares the room and C is outside. B/C must remain
   free unless native perception succeeds; visible B should enter when detected.
2. Move B into view, or have B attack/help a combatant. B joins once, without
   advancing A or executing a free attack/heal first. Ctrl selection stays valid.
3. Keep B's unrelated free healing action in progress as a different participant
   notices B. Its effect/slot must resolve once before B joins. Separately test
   the initial attack target under native interruption rules.
4. Enter with a hidden hero through a door, let several turns pass, and check
   that rectangle scans do not automatically enlist the whole party. An active
   visible foe should still attack valid participants when a hidden PC is closer.
5. Save/reload into a new slot; test ordinary doors, loot and normal non-BG combat.

No game launch/CUA or automatic deployment. The user installs EXE/DLL manually.
The debug scripts and the main D: installation were not modified.
