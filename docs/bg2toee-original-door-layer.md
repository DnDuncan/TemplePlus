# Original Infinity Engine door art: terrain-layer API v1

This opt-in renderer feature draws original source-art PNG patches in terrain
canvas coordinates, before actors. It does not instantiate or adapt ToEE portal
models. No game resources are included in this repository.

Python 2.7 `toee.game` API (on the currently open map):

```
game.bg_door_api_version()  # 1: rendering capability only
game.bg_door_register(id, x, y, width, height,
                     day_closed, day_open, night_closed, night_open, initial_open=0)
game.bg_door_set_state(id, 1)  # open; 0 closes
game.bg_door_get_state(id)
game.bg_door_clear()  # remove only this map's registered patches
```

All rectangle values are **terrain-canvas pixels**, not locXY, BG area pixels
or viewport coordinates. The bg2toee converter must add its existing canvas
offset once. The renderer uses the same terrain origin as the base map: no
second camera projection, resizing or pixel-registration change.

Each texture must be PNG under `art/bg2toee/doors/`, have exactly the registered
dimensions, and exist before registration. Registration validates all four
textures before replacing an existing entry. Dimensions are bounded to 1024
per side and to the 66x71 terrain canvas. IDs are normalized to uppercase;
states are separate per map and per ID. Re-registration preserves the live
state, while game reset and module unload clear it. At most 128 patches per map.

The four patches must share a **binary alpha mask** containing the union of
source day/night pixel differences between open and closed states. This ensures
proper blending during the normal day/night transition, including night light
spill. Unchanged pixels must be transparent so independent non-overlapping door
effects do not overwrite the rest of the map. Use point sampling for native
pixel registration. Do not modify cached terrain JPEG files while the game runs.

Initial validation target: AR2600 DOOR2616, source change rectangle 128x192 at
terrain-canvas (6872,7602), exported from the user's own BGEE installation.

## Scope and verification

This first slice is **rendering only**. It does not implement ARE polygon hit
testing, mouse cursor/use, dynamic search cells, WED actor occlusion, sounds,
linked travel regions, save/load persistence or arbitrary overlapping masks.
Drawing before actors is correct for terrain but is not by itself door-wall
occlusion. Those systems need separate implementation and in-game validation.

Tests/OriginalDoorTests exercises the same platform-independent registry used
by the engine: state isolation, re-registration, path/rectangle validation,
lighting texture indices and reset. CI compiles/runs it before building the
full engine. A successful build is not a graphics/interaction acceptance test.

The bg2toee repository supplies a resource-free exporter and a console test
adapter. Test only in a debug copy; retain the original executable and use a
separate save. Original artwork stays on the player's machine.

## Optional mouse input v1

`bg_door_api_version()` remains 1 for the existing rendering contract. Builds
with mouse switching additionally expose `bg_door_input_version() == 1`:

```
game.bg_door_bind(id, closed_outline, open_outline)
game.bg_door_is_bound(id)
game.bg_door_unbind(id)
```

Outlines are 3..256 `(x,y)` integer terrain-canvas vertices, translated from
the original ARE using the same canvas offset as the image. Bind is opt-in and
requires registered textures. Rejected geometry leaves the old binding intact.
The live state selects the corresponding polygon, with edges included; ambiguous
overlapping doors do not select an arbitrary target. Clearing a map/reset also
clears its input geometry. A thin yellow hover line displays the source outline.

Only non-combat in-game viewport input participates. Native selectable objects
take precedence. Dialog, character/townmap/logbook screens, console, radial menu,
target picker and DM tools retain their input. An owned left press is consumed
through release; modifier-assisted clicks retain their native input behavior.
An unmodified press is consumed
through release; it toggles exactly once only on the same door/map without a
drag beyond four native pixels. Starting on ground and releasing on a door does
not toggle it. Map changes, reset and cancelled/UI-interrupted gestures discard
the press. Input uses scene coordinates already provided by GameView, without
a second window-scale transformation; render and picking share GetTerrainOrigin.

This is still an interaction probe: no automatic actor approach, reach/lock
rules, combat actions, path blocking, travel gating, WED occlusion or persistence.
The existing Python threshold probe remains independent. Do not infer gameplay
blocking from a closed graphic. Mouse input is disabled again by unbind/clear.
Standalone tests cover the actual polygon and press-state code; in-game routing,
hover registration and UI fall-through require a separate user acceptance run.
