#include "stdafx.h"
#include "bgscenecombat.h"
#include "bgengagementrules.h"
#include "turn_based.h"
#include "float_line.h"

#include <temple/dll.h>

#include "combat.h"
#include "tig/tig_mouse.h"
#include "messages/messagequeue.h"
#include "ui_ingame.h"
#include "ui_bgwalk.h"
#include "ui_systems.h"
#include "ui_legacysystems.h"
#include "ui_dialog.h"
#include "gameview.h"
#include "gamesystems/gamesystems.h"
#include "gamesystems/mapsystem.h"
#include "gamesystems/mapobjrender.h"
#include "gamesystems/objects/objsystem.h"
#include "gamesystems/gamerenderer.h"
#include "radialmenu.h"
#include "ai.h"
#include "party.h"
#include "dungeon_master.h"
#include "tig/tig_loadingscreen.h"
#include "tig/tig_console.h"
#include "ui_char.h"
#include "ui_townmap.h"
#include "raycast.h"
#include "hotkeys.h"
#include "infrastructure/keyboard.h"
#include "location.h"
#include "action_sequence.h"
#include "graphics/device.h"
#include "graphics/mapterrain.h"
#include "config/config.h"
#include "maps.h"
#include "ui_picker.h"
#include "python/python_integration_obj.h"

UiInGame::UiInGame(const UiSystemConf &config) {
	auto startup = temple::GetPointer<int(const UiSystemConf*)>(0x10112e70);
	if (!startup(&config)) {
		throw TempleException("Unable to initialize game system Intgame");
	}
	bgwalk::Initialize();
}
UiInGame::~UiInGame() {
	bgwalk::Shutdown();
	auto shutdown = temple::GetPointer<void()>(0x10112eb0);
	shutdown();
}
void UiInGame::ResizeViewport(const UiResizeArgs& resizeArg) {
	auto resize = temple::GetPointer<void(const UiResizeArgs*)>(0x10112ec0);
	resize(&resizeArg);
	bgwalk::Resize();
}
void UiInGame::Reset() {
	CancelOriginalDoorInput();
	if (gameView) gameView->ResetWorldZoom();
	auto reset = temple::GetPointer<void()>(0x101140b0);
	reset();
}
bool UiInGame::LoadGame(const UiSaveFile &save) {
	CancelOriginalDoorInput();
	if (gameView) gameView->ResetWorldZoom();
	auto load = temple::GetPointer<int(const UiSaveFile*)>(0x101140c0);
	return load(&save) == 1;
}
const std::string &UiInGame::GetName() const {
	static std::string name("Intgame");
	return name;
}

void UiInGame::ClearGroupArray()
{
	static auto ClearIntgameGroupArray = temple::GetPointer<int()>(0x10138c70);
	ClearIntgameGroupArray();
}

void UiInGame::AddToGroupArray(objHndl handle)
{
	static auto add_to_intgame_group_arr = temple::GetPointer<void(objHndl)>(0x10138c90);
	add_to_intgame_group_arr(handle);
}

void UiInGame::ResetFocusObject()
{
	static auto ResetIntgameFocusObj = temple::GetPointer<int()>(0x10138c80);
	ResetIntgameFocusObj();
}

void UiInGame::SetFocusObject(objHndl handle)
{
	static auto SetIntgameFocus = temple::GetPointer<void(objHndl)>(0x10138cb0);
	SetIntgameFocus(handle);
}

void UiInGame::ProcessMessage(const TigMsg & source) {
	auto msg = source;
	DoKeyboardScrolling();
	if (HandleWorldZoomWheel(msg)) return;
	if (msg.type == TigMsgType::MOUSE && (msg.arg4 & MSF_RMB_CLICK))
		RadialMenuNoteRmbPress(static_cast<int>(msg.arg1), static_cast<int>(msg.arg2));
	ScopedWorldInput worldInput(&msg);
	if (msg.type == TigMsgType::KEYSTATECHANGE || msg.type == TigMsgType::KEYDOWN
		|| msg.type == TigMsgType::CHAR || msg.type == TigMsgType::EXIT
		|| (msg.type == TigMsgType::WIDGET && reinterpret_cast<const TigMsgWidget&>(msg).widgetEventType == TigMsgWidgetEvent::Exited)
		|| (combatSys.isCombatActive() && !bgscenecombat::FreeSelection()) || gameSystems->GetMap().IsClearingMap()
		|| uiSystems->GetDlg().IsActive() || uiSystems->GetChar().IsVisible()
		|| uiSystems->GetTownmap().IsVisible() || uiSystems->GetLogbook().IsVisible()
		|| tig->GetConsole().IsOpen()) CancelOriginalDoorInput();


	if (dmSys.IsActive()){

		auto actionWasActive = dmSys.IsActionActive();

		if (dmSys.HandleMsg(msg)){
			CancelOriginalDoorInput();
			return;
		}
		dmSys.SetIsHandlingMsg(false);
			

		if (dmSys.IsMoused()){
			CancelOriginalDoorInput();
			return;
		}

		if (msg.type == TigMsgType::MOUSE){
			auto mouseMsg = *(TigMsgMouse*)&msg;
			if ( (mouseMsg.buttonStateFlags & MouseStateFlags::MSF_RMB_CLICK)
				 || (mouseMsg.buttonStateFlags & MouseStateFlags::MSF_RMB_RELEASED) ){

				if (dmSys.IsEditorActive()) // to release edited obj
					return;
				if (dmSys.GetHoveredCritter()) // to acquire edited obj
					return;
			}
		}
		
		if (dmSys.IsActionActive())
			return;

		if (actionWasActive) {
			auto asd = 1;
		}
	}
	
	

    if (!dmSys.IsActive() && !uiSystems->GetDlg().IsActive() && !uiSystems->GetChar().IsVisible()
        && !uiSystems->GetTownmap().IsVisible() && !uiSystems->GetLogbook().IsVisible() && !tig->GetConsole().IsOpen()) {
            if (bgscenecombat::Enabled() && combatSys.isCombatActive() && msg.type==TigMsgType::KEYSTATECHANGE) {
                auto key=reinterpret_cast<const TigKeyStateChangeMsg&>(msg);
                if (!uiPicker.PickerActiveCheck() && radialMenus.GetActiveRadialMenuNode()==-1 && !key.down && key.key>=0x3b && key.key<=0x42 && key.key-0x3b<party.GroupListGetLen()) {
                    auto actor=party.GroupListGetMemberN(key.key-0x3b);
                    if (bgscenecombat::Free(actor) || actor==tbSys.turnBasedGetCurrentActor()) {
                        if (!bgengagement::KeepSelection(hotkeys.IsKeyPressed(VK_CONTROL),bgscenecombat::FreeSelection(),bgscenecombat::Free(actor))) party.CurrentlySelectedClear();
                        party.AddToCurrentlySelected(actor);return;
                    }
                    if (bgscenecombat::Parked(actor)) {bgscenecombat::Activate(actor);return;} // show that hero's encounter
                    if (bgscenecombat::ShowFight(actor)) return; // back to the current turn
                }
            }
			if (bgscenecombat::GlobalCombatActive() && bgscenecombat::FreeSelection()) {
                bgscenecombat::ActorActions actorActions(party.GetCurrentlySelected(0),true);
                if (!actorActions.Ready()) return;
                if (HandleRadialMenuMessage(msg) || HandlePickerMessage(msg)) return;
                if (radialMenus.GetActiveRadialMenuNode()!=-1) return;
                HandleNonCombatMessage(msg);
                return;
            }
    }

	if (HandleRadialMenuMessage(msg)) {
		CancelOriginalDoorInput();
		return;
	}

	if (HandlePickerMessage(msg)) {
		CancelOriginalDoorInput();
		return;
	}

	if (radialMenus.GetActiveRadialMenuNode() != -1) {
		CancelOriginalDoorInput();
		return;
	}

	if (uiSystems->GetDlg().IsActive())
	{
		if (msg.type == TigMsgType::KEYSTATECHANGE)
		{
			InGameKeyEvent kbEvent(msg);
			uiSystems->GetManager().SetState(0);
			uiSystems->GetManager().HandleKeyEvent(kbEvent);
		}
	}
	else
	{
		if (uiSystems->GetChar().GetInventoryObject() && msg.type == TigMsgType::MOUSE && msg.arg4 & 0x44)
		{
			uiSystems->GetChar().SetInventoryObject(objHndl::null);
			mouseFuncs.SetDraggedIcon(0, 0, 0);
		}

		static int* viewportIds = temple::GetPointer<int>(0x10BD3AF8);
		static uint32_t &viewportIdx = temple::GetRef<uint32_t>(0x10BD3B44);
		if (viewportIds[viewportIdx + 1]) CancelOriginalDoorInput();

		
		if (!viewportIds[viewportIdx+1])
		{
			static bool combatModeMsg = false;
			if (combatSys.isCombatActive())
			{
				// First message in combat
				if (!mPrevMsgWasInCombat) {
					mPartyMembersMoving = false;
				}
				mPrevMsgWasInCombat = true;
				HandleCombatMessage(msg);
			}
			else
			{
				mPrevMsgWasInCombat = false;
				HandleNonCombatMessage(msg);
			}
		}
	}

}

bool UiInGame::HandleRadialMenuMessage(const TigMsg &msg) {
	static auto UiRadialMenuMsg = temple::GetPointer<BOOL(const TigMsg &msg)>(0x1013dc90);
	return UiRadialMenuMsg(msg) != 0;
}

bool UiInGame::HandlePickerMessage(const TigMsg &msg) {
	static auto PickerMsgHandler = temple::GetPointer<BOOL(const TigMsg &tigMsg)>(0x101375e0);
	return PickerMsgHandler(msg) != 0;
}

void UiInGame::HandleCombatMessage(const TigMsg & msg)
{
	if (msg.type == TigMsgType::KEYSTATECHANGE)
	{
		HandleCombatKeyStateChange(msg);
	}
	else if (msg.type == TigMsgType::MOUSE)
	{
		if (msg.arg4 & MSF_POS_CHANGE || msg.arg4 & MSF_POS_CHANGE_SLOW) {
			HandleMouseMoveEvent(msg);
		}
	}
}

void UiInGame::HandleNonCombatMessage(const TigMsg & msg)
{
	if (uiSystems->GetChar().IsVisible() && msg.type == TigMsgType::MOUSE)
		return;

	if (msg.type == TigMsgType::KEYSTATECHANGE) {
		return HandleNonCombatKeyStateChange(msg);
	}

	if (msg.type == TigMsgType::MOUSE) {
		if (bgwalk::HandleMouse(msg)) return;
		if (HandleOriginalDoorMessage(msg)) return;

		auto tigMsgMouse = (TigMsgMouse&)msg;
		auto mouseFlags = tigMsgMouse.buttonStateFlags;
		if (mouseFlags & MSF_LMB_RELEASED){
			return temple::GetRef<void(__cdecl)(const TigMsg&)>(0x10114AF0)(msg); // normal LMB released handler
		}
		else if ( (mouseFlags & MSF_LMB_CLICK) || ( (mouseFlags & (MSF_POS_CHANGE | MSF_LMB_DOWN)) == (MSF_POS_CHANGE | MSF_LMB_DOWN))){
			return temple::GetRef<void(__cdecl)(const TigMsg&)>(0x101148B0)(msg); // LMB drag handler
		}
		else if (mouseFlags & MSF_RMB_CLICK){
			return temple::GetRef<void(__cdecl)(const TigMsg&)>(0x10114D90)(msg); // RMB handler
		}
		else if ( (mouseFlags & (MSF_POS_CHANGE)) || (mouseFlags & (MSF_POS_CHANGE_SLOW) ) ){
			//static auto NormalMsgHandler = temple::GetPointer<void(const TigMsg &msg)>(0x10114e30);
			//NormalMsgHandler(msg);
			return HandleMouseMoveEvent(msg);
		}
	}

}

void UiInGame::CancelOriginalDoorInput() {
	mOriginalDoorPress.Cancel();
	if (gameSystems) {
		gameSystems->GetTerrain().SetHoveredOriginalDoor({});
		gameSystems->GetTerrain().SetHoveredOriginalTravel({});
		gameSystems->GetTerrain().ClearDoorContextHover();
		gameSystems->GetTerrain().CancelOriginalDoorUse();
	}
}

bool UiInGame::HandleOriginalDoorMessage(const TigMsg &msg) {
	auto &terrain = gameSystems->GetTerrain();
	if (!terrain.HasOriginalDoorInput()) { CancelOriginalDoorInput(); return false; }
	if (gameSystems->GetMap().IsClearingMap() || (combatSys.isCombatActive() && !bgscenecombat::FreeSelection())
		|| uiSystems->GetChar().IsVisible() || uiSystems->GetChar().IsLevelingUp()
		|| uiSystems->GetChar().GetInventoryObject() || uiSystems->GetTownmap().IsVisible()
		|| uiSystems->GetLogbook().IsVisible() || uiSystems->GetDlg().IsActive()
		|| tig->GetConsole().IsOpen() || mouseFuncs.MouseOutsideWndGet()
		|| infrastructure::gKeyboard.IsKeyPressed(VK_SHIFT)
		|| infrastructure::gKeyboard.IsKeyPressed(VK_CONTROL)
		|| infrastructure::gKeyboard.IsKeyPressed(VK_MENU)
		|| (dmSys.IsActive() && (dmSys.IsEditorActive() || dmSys.IsActionActive()))) {
		CancelOriginalDoorInput(); return false;
	}
	auto flags = msg.arg4;
	if (flags & MSF_LMB_CLICK) terrain.CancelOriginalDoorUse(); // a new command replaces pending intent
	if (flags & (MSF_RMB_CLICK | MSF_RMB_DOWN | MSF_RMB_RELEASED | MSF_MMB_CLICK | MSF_MMB_DOWN | MSF_SCROLLWHEEL_CHANGE)) {
		CancelOriginalDoorInput(); return false;
	}
	auto x = static_cast<int>(msg.arg1), y = static_cast<int>(msg.arg2);
	auto visible = gameView->GetWorldViewport();
	if (x < visible.left || y < visible.top || x >= visible.right || y >= visible.bottom) {
		CancelOriginalDoorInput(); return false;
	}
	// Source portals use their reviewed floor approach, not the mesh centre.
	// RMB is deliberately left to the native skills wheel above.
	auto contextHit = terrain.HasDoorContext() ? terrain.UpdateDoorContextHover(x,y) : std::string();
	auto sourceDoor = terrain.SourceDoorFor(objHndl{terrain.PickSourceContainer(x,y)});
	if (!sourceDoor.empty() && (contextHit.empty() || contextHit[0] != '!')) {
		int ox,oy; terrain.GetTerrainOrigin(ox,oy);
		using Event=OriginalDoorMousePress::Event;
		auto event=(flags&MSF_LMB_CLICK) ? Event::Down : (flags&MSF_LMB_RELEASED) ? Event::Up : Event::Move;
		auto result=mOriginalDoorPress.Process(maps.GetCurrentMapId(),sourceDoor,event,x-ox,y-oy,
			(std::max)(4,int(std::ceil(6/gameView->GetWorldZoom()))));
		if (!result.toggle.empty()) {
			try { terrain.RequestOriginalDoorUse(sourceDoor,party.GetConsciousPartyLeader().handle); }
			catch (const std::exception &e) { logger->info("Source door approach refused: {}",e.what()); }
		}
		return result.consumed;
	}
	if (terrain.HasDoorContext()) {
		auto hit=contextHit;
		int ox,oy;terrain.GetTerrainOrigin(ox,oy);using Event=OriginalDoorMousePress::Event;
		auto event=(flags&MSF_LMB_CLICK) ? Event::Down : (flags&MSF_LMB_RELEASED) ? Event::Up : Event::Move;
		auto result=mOriginalDoorPress.Process(maps.GetCurrentMapId(),hit,event,x-ox,y-oy,
			(std::max)(4,int(std::ceil(6/gameView->GetWorldZoom()))));
		if (!result.toggle.empty() && result.toggle[0]=='#') {
			try {terrain.RequestDoorContextDefault(result.toggle.substr(1));}
			catch (const std::exception &e) {logger->info("Door default refused: {}",e.what());}
		}
		if (!result.toggle.empty() && result.toggle[0]=='!' && result.toggle.size()>3) {
			try {terrain.RequestDoorContextAction(result.toggle.substr(3),result.toggle[1]-'0');}
			catch (const std::exception &e) {
				logger->info("Door context refused: {}",e.what());
				auto actor=party.GetConsciousPartyLeader();
				if (actor) floatSys.floatMesLine(actor,1,FloatLineColor::Yellow,e.what());
			}
		}
		if (!hit.empty()) {ResetFocusObject();ClearGroupArray();}
		return result.consumed;
	}
	// Explicit markers and the stable door frame must remain clickable even
	// when the approaching actor stands in front of them.
	auto travel = terrain.HitTestOriginalTravel(x, y);
	auto hit = travel.empty() ? terrain.HitTestOriginalDoor(x, y) : std::string();
	if (!terrain.HasOriginalTravel() && !hit.empty() && GetMouseTarget(x, y)) hit.clear();
	terrain.SetHoveredOriginalDoor(hit);
	terrain.SetHoveredOriginalTravel(travel);
	if (!travel.empty()) hit = "@" + travel;
	int originX, originY; terrain.GetTerrainOrigin(originX, originY);
	using Event = OriginalDoorMousePress::Event;
	auto event = (flags & MSF_LMB_CLICK) ? Event::Down : (flags & MSF_LMB_RELEASED) ? Event::Up : Event::Move;
	auto result = mOriginalDoorPress.Process(maps.GetCurrentMapId(), hit, event, x - originX, y - originY,
		terrain.HasOriginalTravel() ? (std::max)(4, int(std::ceil(6 / gameView->GetWorldZoom()))) : 4);
	if (!result.toggle.empty()) {
		try {
			if (result.toggle[0] == '@') {
				terrain.RequestOriginalTravel(result.toggle.substr(1), party.GetConsciousPartyLeader().handle);
			} else if (terrain.HasOriginalDoorUse(maps.GetCurrentMapId(), result.toggle)) {
				terrain.RequestOriginalDoorUse(result.toggle, party.GetConsciousPartyLeader().handle);
			} else {
				auto open = !terrain.GetOriginalDoorOpen(result.toggle);
				terrain.SetOriginalDoorOpen(result.toggle, open);
				logger->info("Original door click: map={} id={} open={}", maps.GetCurrentMapId(), result.toggle, open);
			}
		} catch (const std::exception &e) {
			logger->info("Original door refused: {}", e.what());
		}
	}
	if (!hit.empty()) { ResetFocusObject(); ClearGroupArray(); }
	return result.consumed;
}

bool UiInGame::HandleWorldZoomWheel(const TigMsg &msg) {
	if (msg.type != TigMsgType::MOUSE || !(msg.arg4 & MSF_SCROLLWHEEL_CHANGE)) return false;
	static auto viewportIds = temple::GetPointer<int>(0x10BD3AF8);
	static auto &viewportIdx = temple::GetRef<uint32_t>(0x10BD3B44);
	if (viewportIds[viewportIdx + 1] || maps.GetCurrentMapId() == 5000
		|| gameSystems->GetMap().IsClearingMap() || uiSystems->GetDlg().IsActive()
		|| uiSystems->GetChar().IsVisible() || uiSystems->GetTownmap().IsVisible()
		|| uiSystems->GetLogbook().IsVisible() || tig->GetConsole().IsOpen()
		|| uiPicker.PickerActiveCheck() || radialMenus.GetActiveRadialMenuNode() != -1
		|| mouseFuncs.MouseOutsideWndGet()
		|| (dmSys.IsActive() && (dmSys.IsMoused() || dmSys.IsEditorActive() || dmSys.IsActionActive()))
		|| infrastructure::gKeyboard.IsKeyPressed(VK_CONTROL)
		|| infrastructure::gKeyboard.IsKeyPressed(VK_SHIFT)
		|| infrastructure::gKeyboard.IsKeyPressed(VK_MENU)
		|| (msg.arg4 & (MSF_LMB_DOWN | MSF_RMB_DOWN | MSF_MMB_DOWN))) return false;
	auto x = static_cast<int>(msg.arg1), y = static_cast<int>(msg.arg2);
	if (x < 0 || y < 0 || x >= config.renderWidth || y >= config.renderHeight) return false;
	CancelOriginalDoorInput();
	if (gameView->ZoomWorldByWheel(static_cast<int>(msg.arg3)))
		logger->info("Camera wheel zoom: {} percent", int(std::round(gameView->GetWorldZoom() * 100)));
	return true;
}

void UiInGame::HandleNonCombatKeyStateChange(const TigMsg& msg){
	//return temple::GetRef<void(__cdecl)(const TigMsg&)>(0x101130B0)(msg); // normal keystate change handler
	InGameKeyEvent kmsg(msg);
	auto msga = reinterpret_cast<const TigKeyStateChangeMsg&>(msg);
	if (msga.down){
		return;
	}

	// BG journal (bg2toee): J, as in Baldur's Gate. The mod's Python adapter
	// decides what to show; without the mod the key keeps its ToEE meaning.
	constexpr uint32_t DikJ = 0x24; // DIK_J
	if (msga.key == DikJ && !hotkeys.IsKeyPressed(VK_CONTROL) && !hotkeys.IsKeyPressed(VK_SHIFT)
		&& !hotkeys.IsKeyPressed(VK_MENU)) {
		auto args = Py_BuildValue("()");
		auto result = pythonObjIntegration.ExecuteScript("templeplus.savehook", "journal_key", args);
		Py_DECREF(args);
		bool handled = result && PyObject_IsTrue(result) == 1;
		Py_XDECREF(result);
		if (handled) return;
	}

	// BG rest (bg2toee): R, as in Baldur's Gate, opens the camp on BG maps.
	constexpr uint32_t DikR = 0x13; // DIK_R
	if (msga.key == DikR && !hotkeys.IsKeyPressed(VK_CONTROL) && !hotkeys.IsKeyPressed(VK_SHIFT)
		&& !hotkeys.IsKeyPressed(VK_MENU)) {
		auto restArgs = Py_BuildValue("(i)", 2);
		auto restResult = pythonObjIntegration.ExecuteScript("templeplus.savehook", "rest_key", restArgs);
		Py_DECREF(restArgs);
		bool restHandled = restResult && PyObject_IsTrue(restResult) == 1;
		Py_XDECREF(restResult);
		if (restHandled) return;
	}

	auto leader = party.GetConsciousPartyLeader();
	if (leader){
		if (hotkeys.IsReservedHotkey(msga.key)){
			if (hotkeys.IsKeyPressed(VK_LCONTROL) || hotkeys.IsKeyPressed(VK_RCONTROL)){ // trying to assign hotkey to reserved hotkey
				hotkeys.HotkeyReservedPopup(msga.key);
				return;
			}
		}
		else if (hotkeys.IsNormalNonreservedHotkey(msga.key)){
			if (hotkeys.IsKeyPressed(VK_LCONTROL) || hotkeys.IsKeyPressed(VK_RCONTROL)) { // assign hotkey
				auto leaderLoc = objects.GetLocationFull(leader);
				auto screenPos = gameView->WorldToScreenUi(leaderLoc.ToInches3D());

				radialMenus.SpawnMenu(int(screenPos.x), int(screenPos.y));
				radialMenus.MsgHandler(&msg);
				return;
			}
			
			actSeqSys.TurnBasedStatusInit(leader);
			leader = party.GetConsciousPartyLeader();// in case the leader changes somehow...
			logger->info("Intgame: Resetting sequence.");
			actSeqSys.curSeqReset(leader);
			
			d20Sys.GlobD20ActnInit();
			auto radmenuHotkeySthg = temple::GetRef<RadialMenu*(__cdecl)(objHndl, int)>(0x100F3D60);
			if (radmenuHotkeySthg(party.GetConsciousPartyLeader(), msga.key))
			{
				actSeqSys.ActionAddToSeq();
				actSeqSys.sequencePerform();
				auto fellowPc = party.GetFellowPc(leader);

				char voicelineText[1000];
				voicelineText[0] = 0;
				int soundId = 0;
				critterSys.GetOkayVoiceLine(leader, fellowPc, voicelineText, &soundId);
				critterSys.PlayCritterVoiceLine(leader, fellowPc, voicelineText, soundId);
				radialMenus.ClearActiveRadialMenu();
				return;
			}
			
		}
	}

	uiSystems->GetManager().SetState(0);
	uiSystems->GetManager().HandleKeyEvent(kmsg);
}

void UiInGame::HandleCombatKeyStateChange(const TigMsg & msg)
{
	static auto CombatKeystateHandler = temple::GetPointer<void(const TigMsg &msg)>(0x101132b0);
	CombatKeystateHandler(msg);
}

// was 0x10114690
void UiInGame::HandleMouseMoveEvent(const TigMsg & msg)
{
	uiSystems->GetParty().SetHoveredObj(objHndl::null);
	uiSystems->GetParty().SetPressedObj(objHndl::null);

	uiSystems->GetInGame().ResetFocusObject();
	uiSystems->GetInGame().ClearGroupArray();

	if (gameSystems->GetMap().IsClearingMap()) {
		return;
	}

	auto mouseTarget = GetMouseTarget(msg.arg1, msg.arg2);

	if (!mouseTarget) {
		return;
	}

	if (uiSystems->GetChar().IsVisible()
		|| uiSystems->GetTownmap().IsVisible()
		|| uiSystems->GetLogbook().IsVisible()) {
		return;
	}

	if (uiSystems->GetChar().IsLevelingUp()) {
		return;
	}

	auto normalLmbClicked = temple::GetRef<BOOL>(0x10BD3B5C);	
	if (normalLmbClicked == 1)
	{
		auto dragViewport = temple::GetRef<int>(0x10BD3AC8);
		auto mouseDragTarget = temple::GetRef<objHndl>(0x10BD3AD8);
		if (!dragViewport && mouseTarget == mouseDragTarget)
		{
			uiSystems->GetInGame().AddToGroupArray(mouseTarget);
			uiSystems->GetParty().SetPressedObj(mouseTarget);
		}		
	} else if (objects.IsEquipment(mouseTarget) 
		|| objects.IsContainer(mouseTarget)
		|| objects.IsPortal(mouseTarget)
		|| objSystem->GetProtoId(mouseTarget) == 2064
		) {
		// 2064 is the guestbook
		uiSystems->GetInGame().SetFocusObject(mouseTarget);
	} else if (objects.IsCritter(mouseTarget)) {
		uiSystems->GetInGame().SetFocusObject(mouseTarget);
		if (party.IsInParty(mouseTarget)) {
			uiSystems->GetParty().SetHoveredObj(mouseTarget);
		}
	} else if (objects.GetType(mouseTarget) == obj_t_scenery) {
		if (objects.getInt32(mouseTarget, obj_f_scenery_teleport_to)) {
			uiSystems->GetInGame().SetFocusObject(mouseTarget);
		}
	}

}

objHndl UiInGame::GetMouseTarget(int x, int y)
{
	// Pick the object using the screen coordinates first
	auto mousedOver = PickObject(x, y);
	if (!mousedOver) {		
		return objHndl::null;
	}

	if (objects.IsUntargetable(mousedOver)) {
		return objHndl::null;
	}

	if (objects.IsCritter(mousedOver) && critterSys.IsConcealed(mousedOver)) {
		return objHndl::null;
	}

	return mousedOver;
}

void UiInGame::DoKeyboardScrolling(){
	
	static auto scrollRefTime = timeGetTime();
	auto now = timeGetTime();
	if (now < scrollRefTime + 16){
		auto scrollButter = temple::GetRef<int>(0x102AC238);
		if (!scrollButter)
			return;
	}
	scrollRefTime = now;
	auto &console =tig->GetConsole();
	if (console.IsOpen() && console.InputIsActive()){
		return;
	}
	// don't scroll if game is out of focus (e.g. debugging...)
	if (tig->GetMainWindow().GetHwnd() != GetFocus()) {
		return;
	}
	static auto doKeyboardScrolling = temple::GetPointer<void()>(0x10113fb0);
	doKeyboardScrolling();

}

objHndl UiInGame::PickObject(int x, int y, GameRaycastFlags flags)
{
	objHndl result = objHndl::null;
	if (PickObjectOnScreen(x, y, &result, flags) == 0) {
		return objHndl::null;
	} else {
		return result;
	}
}

void UiInGame::ResetInput()
{
	CancelOriginalDoorInput();
	static auto ui_intgame_reset_input = temple::GetPointer<void()>(0x10112f10);
	ui_intgame_reset_input();
}
