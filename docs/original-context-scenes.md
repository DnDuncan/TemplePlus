# Context door actions and selected-group scene probe

Stacked on the accepted click-travel probe. The source door receives a blue
outline on hover. After 450 ms a transparent, texture-based action palette fades
in over 150 ms; a 350 ms leave grace allows reaching the buttons. Closed/unlocked:
Open. Open: Close and Travel. Locked: Search and Pick Lock. Fixed 48 display-pixel
icons replace the permanent arrow, with 68% opacity and a stronger hover tint.
Artwork is newly authored outside this repository, never extracted game icons.

Search/Open Lock choose the conscious selected actor with the highest dispatched
skill bonus, respecting trained-only skill properties, disabled abilities and
native skill swaps. Class is not a selection criterion. Ties prefer the nearer
actor. That actor approaches, then makes one native skill roll. A changed
selection, map, revision, combat state or invalid actor cancels the request.
Successful picking unlocks without opening. Searching can discover a trap;
triggering/removing original IE trap scripts is outside this map-production probe.

## Why isolated areas share a native scene

The native teleport process adds the whole party, reloads a single map, and
rebuilds native party references. Filtering the teleport list would leave stale
references to unloaded actors. This pilot instead translates the inn by exactly
192 native tiles along BOTH world axes: 5376 canvas pixels down (21 JPG tiles),
with no scale/rotation/resampling. Its static sector translates from (7,7) to
(10,10); the original sector body is unchanged. Original AR2616 ground textures
are sampled at their original tile coordinates. The outdoor image/navigation are
untouched. A townmap extension displays the two islands.

The native map remains 5136; logical area IDs are 5136 and 5190. Source rectangles
are disjoint and blocked space separates them. View limits and focus follow the
first conscious selected portrait. A new scene API plans free arrival positions
for EVERY selected conscious actor, then moves only those existing objects on
the same native map. No actor, ID, inventory, party membership or dispatcher is
copied or replaced. Unselected members retain their locations and normal native
save data. A changed/mixed-area selection cancels travel instead of collecting
remote members. All-selected remains a normal whole-group move.

This is a non-combat Candlekeep/inn pilot, not a general multi-map simulation.
Native combat/time/spell/rest systems still belong to one scene. Additional
campaign areas, per-area simulation rules and independent battle scopes require
separate design and acceptance. No general cross-map selective teleport is added.

## Validation

Standalone tests cover action availability, stationary/delayed hover, leave grace,
clock wrap, negative bonuses, best eligible actor independent of class, ties,
selection identity, bounded rectangles and exact scene translation. Full native
compilation and existing engine tests run in CI. The source project checks ARE
security import, save metadata, packaging, byte-identical sector translation and
selected travel dispatch. These checks do not constitute interactive game tests.
No computer use is authorized; visual, party and save-load acceptance are pending.
