#pragma once
#include "common.h"
namespace bgscenecombat {
bool Enabled();
int Scene();
void Begin(objHndl actor);
void Started();
void SeedInitiative();
void BindCivilian(objHndl actor);
bool Civilian(objHndl actor);
bool MayJoin(objHndl actor);
bool Free(objHndl actor);
bool Busy(objHndl actor);
bool FreeSelection();
bool CanTarget(objHndl actor,objHndl target);
void Sleep(objHndl actor,bool sleeping);
void Pulse(uint32_t now);
void Turn();
bool MoveFreeSelection(LocAndOffsets target);
}
