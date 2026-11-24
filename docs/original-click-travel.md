# Original door click travel probe

Adds explicit, vector-drawn entry/exit arrows to the optional BG original-art
probe. The markers have a 60 display-pixel hit diameter at 50–230% zoom and draw
above actors. A closed gate is grey and crossed out. Clicks on registered markers
and the stable door frame take precedence over actors behind those hotspots.
Original source PNGs, outlines, navigation masks and terrain are unchanged.

Door use can use a five-foot range supplied by the new source profile. Its line
test excludes only its own dynamic door mask; static walls and other doors still
block it. Door state changes still reject newly occupied blocker cells. Refusals
are logged and report a short floating message.

`bg_travel_bind(map, route, gate_map, gate_id, canvas_x, canvas_y, direction_x,
direction_y, target_world_x, target_world_y, reach=60)` registers a bounded marker.
`bg_travel_peek()` returns None or `(serial, map, route)` only after the actor has
approached. `bg_travel_ack(serial)` consumes it once. Python validates the route,
open gate, combat and whole-party proximity before teleporting. A new command,
map change, registry revision, invalid actor, combat or timeout cancels intent.
No movement or click intent is serialized. Save restoration rebinds marker
geometry through the versioned Python lifecycle adapter.

Standalone tests cover stable open/closed hits, ambiguous overlaps, scoped mask
exclusion, marker direction/size/hit at both zoom limits, one-shot acknowledgments,
expiry, clock wrap, replacement and cancellation. Full engine compilation and
packaging run in CI. Interactive behavior and visual placement need Windows game
acceptance; this change has not been checked with computer use.
