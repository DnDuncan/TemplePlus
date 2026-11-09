#pragma once
#include "common.h"
#include <memory>
#include "bgactorcontext.h"
namespace bgscenecombat {
void ResetCombatActions();
bool GlobalCombatActive();
bool ContextCombatActive();
extern BgActorContext<objHndl> actorContext;
// Preserve the native combat cursor while another actor builds or finishes an action.
class ActorActions {
public:
    explicit ActorActions(objHndl actor,bool prepareInput=false);
    ~ActorActions();
    bool Ready() const;
    ActorActions(const ActorActions&)=delete;
    ActorActions& operator=(const ActorActions&)=delete;
private:
    struct State;
    BgActorContext<objHndl>::Scope queryScope;
    std::unique_ptr<State> state;
    bool ready=true;
};
bool Enabled();
int Scene();
void Begin(objHndl actor);
void Started();
void SeedInitiative();
void BindCivilian(objHndl actor);
bool Civilian(objHndl actor);
bool MayJoin(objHndl actor);
bool MayAutoJoin(objHndl actor);
void RecruitParty();
void AfterJoin(objHndl actor);
void ResetEngagement();
bool PrimaryParticipant(objHndl actor);
bool DeferJoin(objHndl actor);
bool Free(objHndl actor);
bool Busy(objHndl actor);
bool FreeSelection();
bool CanTarget(objHndl actor,objHndl target);
void Sleep(objHndl actor,bool sleeping);
void Pulse(uint32_t now);
void Turn();
bool MoveFreeSelection(LocAndOffsets target);
// Separate encounters: ToEE shows one, every other one waits parked on a hero's turn.
bool Parked(objHndl actor);
unsigned Encounters();
bool ParkForSeparate(objHndl initiator);
bool Activate(objHndl actor);
bool ResumeParked();
unsigned Shown(); // changes whenever another encounter is shown
void DiscardEncounters();
// A fight belongs to its members only. While free heroes are selected and the
// shown fight waits on a hero's turn, it is parked and the rest of the world
// leaves combat (music, initiative UI, dialogue, travel). It is shown again when
// one of its heroes is selected or one of its enemies notices a free hero.
bool ParkForDialog(objHndl hero);
// Choosing a fighter (portrait/F-key) while free heroes are selected gives control back to the turn.
bool ShowFight(objHndl actor);
}
