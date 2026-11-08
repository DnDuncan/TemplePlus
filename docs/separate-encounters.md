# Separate BG encounters (parked encounter contexts)

Branch `claude/encounter-contexts`, based on PR32 / `80e3a34` (0.2.146).
Source and native rule tests only; no gameplay result is claimed.

## Model

ToEE keeps one encounter in native globals. This package adds a registry
(`TemplePlus/bgencounters.h`) of *parked* encounters, each holding its own:

- initiative list (members, order, sort function) and current actor;
- current initiative value, surprise flag and round counter;
- the current hero's remaining action budget (`TurnBasedStatus`);
- the readied-action cache.

An actor belongs to at most one encounter. A parked actor cannot join the shown
encounter, is not free, cannot act out of turn and cannot be targeted from the
shown encounter. Logical scenes still only filter who may start or join.

## Lifecycle

- **Second encounter.** A free hero (or free NPC, never a civilian) whose hostile
  action cannot join the shown encounter now parks it and opens a new native
  encounter through the normal realtime combat trigger. Its targets enter combat
  and the opener goes first; the opening command is not performed for free and is
  repeated on that turn. If the new encounter fails to start, the parked one resumes.
- **Switching.** F1–F8 on a parked hero shows that hero's encounter and parks the
  shown one. The waiting hero keeps its queue position, round and remaining budget.
- **Joining a waiting encounter.** A free hero's hostile or assisting action on a
  parked member shows that encounter and enters its initiative.
- **End.** When the shown encounter ends, the most recently parked one with living
  members is shown again without a new turn start, round or GameTime step.
- **Safety.** Parking/switching happens only while the shown encounter waits for a
  hero: no member performing, no simultaneous actions, no interrupt/readied
  sequence pending, no dialogue. Otherwise the request is ignored and logged.

## Limits

- A parked encounter is paused, not running in parallel: its NPC turns advance only
  while it is shown. Every encounter can still advance independently in turn.
- Saving is refused while any encounter is parked (logged); serialization by stable
  object IDs is the next step. Saves with one encounter are unchanged. Load and
  combat-system reset discard parked encounters.
- GameTime/effect clocks, dialogue and skill-check contexts remain shared.
- New separate encounters start only from a free actor's hostile action against a
  different logical scene; same-scene separate groups are not opened yet.
- Switching is bound to F1–F8; portrait clicks keep native behavior.

## Manual checks (user)

Use a save made before any combat.

1. A fights in one room; B, in another room, attacks a hostile NPC. A second
   encounter starts with B first; A's encounter waits on A's turn.
2. F-key on A: A's turn, budget and round are as left. F-key on B: B's encounter as left.
3. End B's encounter: A's encounter continues without skipping A or adding a round.
4. Try to save with two encounters: refused. After one ends, saving works.
5. Ordinary one-encounter fights, free actions, doors and loot without regressions.
