# Actor-local actions alongside initiative (draft)

Priority from the user, 8 November 2026: while one part of the party fights,
other heroes must be able to act freely. The user reports having checked
TemplePlus 0.2.138 / engine PR30. This is not evidence for the new changes.

## Architecture in this package

Keep the encounter flag, initiative list and current turn globally authoritative.
Use a nested actor query context only during ordinary input, action preparation,
cost calculation and animation/projectile callbacks. A detached actor sees
realtime action rules; a participant sees combat rules. Membership is evaluated
each time, including after a scene join. No temporary writes to combatModeActive.
Python combat_is_active continues to report the global encounter.

A detached actor borrows the native sequence cursor with a snapshot of the D20
action, interrupted sequence and default-action/targeting fields. The previous
native sequence is reserved against allocation until return. Reservations live
outside native flags and unwind with exceptions. Targeting retains its native
sequence until completion. Native start/end resets preserve in-flight sequences
and their fixed path-cache entries for nonparticipants. Combat-end signals and
animation interrupts are limited to participants in BG scenes.

Free selection uses ordinary input, radial menu and targeting handlers. A hostile
command in the active encounter's scene joins initiative before performance,
without executing the command for free. Commands targeting another logical scene
are refused. Existing scene transitions continue to join entering characters.

## Scope and remaining work

This is a draft first package for one initiative encounter plus realtime actors.
It does not implement several simultaneous independent encounter lists. A hostile
command outside the current encounter is deferred without consuming its action;
a second encounter scheduler is still needed. World GameTime clocks and multiple
simultaneous dialogues/pickers remain shared. Do not claim full BG3 equivalence.
Scripted automatic greetings still have the prior global combat deferral.

## Verification

- Native policy tests cover nested participant/realtime contexts, membership
  changes, encounter end, exception unwinding and sequence reservations.
- Seven Python savehook tests pass locally. Native compilation and integration
  must pass CI; gameplay remains unverified. No local Windows C++ compiler found.
- No game launch, computer use, automatic EXE installation or debug script writes.
  The installed 0.2.138 baseline is unchanged. API3 identifies this native draft;
  existing API2 scripts remain compatible, but have their documented limitations.

Manual checks after a successful CI build and manual EXE setup: keep hero A in
initiative and select hero B outside; move, open a door, use an item, cast healing,
select/cancel a spell target, and switch back to A. A's action budget and queue
must remain unchanged. Repeat while B's animation finishes during A's turn,
while combat starts/ends, and when B joins the active room. Save/reload into a
new slot and verify the normal outside-combat controls and other ToEE maps.
