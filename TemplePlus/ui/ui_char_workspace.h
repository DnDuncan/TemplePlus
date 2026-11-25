#pragma once

struct TigMsg;

namespace charworkspace {

// True while the workspace owns the inventory session's presentation.
bool Render();
bool HandleInput(const TigMsg &msg);
void Reset();

}
