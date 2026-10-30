# Mouse-wheel world zoom

Wheel up/down over the game world changes a presentation-only zoom between
50% and 200%, centered on the scene. Six full wheel notches double/halve it;
partial high-resolution deltas accumulate. Start/load/reset uses 100%.
Source art, area coordinates, navigation, camera elevation and native tile
projection are unchanged. Magnifying a background does not add image detail.

The world pass applies the same affine scale to the 3D projection, 2D terrain,
original door patches/outlines and DirectWrite world text. HUD rendering restores
unit presentation scale. Culling uses the inverse visible rectangle, including
negative logical pixels when zoomed out, rather than cropping to the old view.

Native world input sees inverse-transformed coordinates in scoped copies of the
event and mouse state. Scope nesting prevents a second transform; original
messages and mouse state are restored before HUD dispatch, hardware cursor and
screen-edge scrolling. Logical ray picking remains separate from GPU projection.
Door input uses the expanded logical viewport. UI scrollbars, dialog, inventory,
town map, logbook, console, radial menu, target picker, DM tools and modified/
button-held wheel gestures keep their input.

Optional game console API: `game.bg_camera_zoom()` queries zoom;
`game.bg_camera_zoom(1.0)` resets it. Set values must be finite and within
0.5..2.0; they snap to the wheel's steps. No save/config persistence is added.

Camera tests exercise the actual matrices for identical 2D/3D transforms,
logical ground picking and single-axis resizing. Portable zoom tests cover
round trips, visible extents, high-resolution input and clamping. These do not
replace manual/game checks: wheel routing, world/HUD registration, ground
movement, native actor selection, original-door clicking at both limits, menus,
day/night and map changes must be recorded separately.

The accepted original-door renderer/input runtime is retained for rollback.
Build and test only in the debug installation; no game resources are included.
