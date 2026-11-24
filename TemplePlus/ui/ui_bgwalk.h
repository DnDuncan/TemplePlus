#pragma once
struct TigMsg;
namespace bgwalk {
void Initialize();
void Shutdown();
void Resize();
void Update();
void SetUtilityVisible(bool visible);
bool HandleHotkey(const TigMsg &msg);
bool HandleMouse(const TigMsg &msg);
}
