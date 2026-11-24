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
- **The world outside a fight (0.2.153).** A fight belongs to its members. While
  only free heroes are selected and the shown fight waits on a hero's turn, it is
  parked and global combat ends: normal music, no initiative bar, no turn path
  preview, native dialogue, travel and interactions for everyone else. A free
  hero's dialogue parks it the same way. The parked fight is shown again when one
  of its heroes is selected (portrait, F-key or click in the world) or when one of
  its enemies threatens or notices a free hero (40 ft since 0.2.164, Perception vs Stealth);
  that hero then joins its initiative.
- **Switching.** F1–F8 or a portrait on a parked hero shows that hero's encounter
  and parks the shown one. The waiting hero keeps its queue position, round and
  remaining budget.
- **Joining a waiting encounter.** A free hero's hostile or assisting action on a
  parked member shows that encounter and enters its initiative. Since 0.2.163
  (phase 2) this also works while the world is out of combat (before, a parked
  member could not be targeted then). The newcomer rolls initiative (d20 + mods)
  into the existing order: a slot already passed this round acts next round.
  It is flat-footed until its first turn, never Surprised, and the others keep
  their initiative, round and turn.
- **Unseen attacker (0.2.163).** A sneaking or hidden hero whose target has not
  noticed it (BG perception, Spot vs Hide / Listen vs Move Silently) strikes
  unseen for that action: +2 to hit, the target loses its Dexterity and dodge AC
  bonuses ("Unseen Attacker", bonus.mes 357) and sneak attack applies. It is
  decided when the action starts, before the attack reveals the hero; logged
  "BG unseen attack". No surprise round for the rest of the fight.
- **End.** When the shown encounter ends, waiting ones stay parked (the world is
  out of combat) until one of their heroes is selected or a free hero is noticed;
  then it continues without a new turn start, round or GameTime step.
- **Safety.** Parking/switching happens only while the shown encounter waits for a
  hero: no member performing, no simultaneous actions, no interrupt/readied
  sequence pending, no dialogue. Otherwise the request is ignored and logged.
- **Downed heroes (0.2.158, phase 1 of encounter-continuity-design.md).** A downed
  hero's turn never waits for input: it is skipped while the fight has an active
  hero ("BG turn skipped"). When every hero of the fight is down and another hero
  is up, the fight is parked unattended ("every hero of the fight is down"): the
  world leaves combat and the fight keeps its own clock, one round per 6 seconds
  outside dialogue, logged "round N goes on without its heroes" (dying and effect
  durations are ToEE's own real-time rounds, which run every 6 s out of combat;
  0.2.158 also dispatched BeginRound, which doubled them and was removed). Selecting a downed hero does not
  show it; a free hero noticed by its enemies joins its order (no restart, no new
  initiative for the others). A raised hero makes it an ordinary waiting fight; it
  ends when its heroes are dead or its enemies are down. While a fight has living
  heroes (downed too), targetless enemies stay in it, and only the fight's own
  heroes count for "enemies far from the party". Subturn keeps a free selection.
- **Enemies of a heroless fight (phase 3).** Each of its rounds the enemies act in
  initiative order: with no conscious hero within 40 ft, an enemy runs to the
  nearest downed hero of its fight and, once in reach, performs a coup de grace
  ("finishes downed ... (coup de grace)", float and combat log); otherwise it
  holds (design 9.1 a).
- **Stealth feedback.** An attack is judged unseen when the action starts (it was
  judged after the roll before), with a fresh perception chance. An unalerted
  enemy (out of every fight, AI not fighting) with the hero in its rear half has
  Spot -5; sight stays all-round, Listen is unchanged. Text is in
  tpdata/tpmes/bg_stealth.mes (cp1251): the combat log shows Spot vs Hide and
  Listen vs Move Silently with every modifier whenever an outcome changes or an
  attack depends on it; '?' (missed) and '!' (noticed) float over enemies and
  Hidden/Noticed/Revealed over the hero; "Attack from stealth!" and a log line
  mark an unseen attack. Holding Shift with a sneaking hero selected draws each
  nearby enemy's 40 ft range: grey unchecked, yellow missed, red noticed; for an
  unalerted enemy a line splits off its rear half and a tick shows its facing.
- **Saving.** Waiting fights are saved in Save\Currentg_encounters.dat inside
  the archive: members, initiative order, turn, round, surprise, the waiting
  hero's budget and readied actions, by persistent object ids. Loading restores
  them after the objects exist (missing members are dropped; a fight with none
  left is not restored). Older saves have no file and load as before.
- **Stances and scenes (0.2.160).** A hero's combat stance is its own: entering
  combat no longer sets the whole party's stance in the BG map, and heroes outside
  every fight drop it (on parking and each pulse while a fight exists). A native
  spread-out move that would leave the actor's isolated scene is undone (logged
  "BG spread-out kept").
- **Empty fights (0.2.156).** Civilians never start a fight. A fight that starts
  with nobody in it (everyone it could involve waits in a parked fight or is in
  another scene) ends at once; one found later by the pulse ends too, logged
  "BG combat ended: nobody is in it". In 0.2.155 such a fight, started by a
  civilian right after parking, held the world in combat with no turn: no
  dialogue, no joining, and the parked fight could not be shown again.

## Limits

- A parked encounter is paused, not running in parallel: its NPC turns advance only
  while it is shown. Every encounter can still advance independently in turn.
- A save is refused only if a member of a waiting fight has no persistent id
  (logged). The unattended round clock restarts at load. Combat-system reset
  discards waiting fights.
- GameTime/effect clocks, dialogue and skill-check contexts remain shared.
- New separate encounters start only from a free actor's hostile action against a
  different logical scene; same-scene separate groups are not opened yet.
- While a fight waits on an NPC's turn it stays shown; it is parked only on a
  hero's turn, so free heroes see combat music/UI during enemy turns.

## Manual checks (user)

Use a save made before any combat.

1. A fights in one room; B, in another room, attacks a hostile NPC. A second
   encounter starts with B first; A's encounter waits on A's turn.
2. F-key on A: A's turn, budget and round are as left. F-key on B: B's encounter as left.
3. End B's encounter: the world is out of combat; select A: A's encounter continues
   without skipping A or adding a round.
4. Save with a waiting fight, load: "BG encounters loaded: N of N"; selecting its
   hero continues the same turn, round and budget.
5. Ordinary one-encounter fights, free actions, doors and loot without regressions.
6. On A's turn select a free hero: combat music and initiative bar end, the hero
   talks, travels and loots; walking into A's room in view of an enemy brings A's
   fight back with the hero in its initiative.
