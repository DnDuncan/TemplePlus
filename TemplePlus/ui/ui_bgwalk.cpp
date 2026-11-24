#include "stdafx.h"
#include "bgscenecombat.h"
#include "ui_bgwalk.h"
#include "ui.h"
#include "ui_render.h"
#include "ui_systems.h"
#include "ui_char.h"
#include "ui_dialog.h"
#include "ui_picker.h"
#include "ui_bgrolls.h"
#include "bgmovementrules.h"
#include "config/config.h"
#include "tig/tig_startup.h"
#include "tig/tig_console.h"
#include "tig/tig_mouse.h"
#include "gamesystems/gamesystems.h"
#include "graphics/mapterrain.h"
#include "graphics/shaperenderer2d.h"
#include "infrastructure/keyboard.h"
#include "party.h"
#include "combat.h"
#include "critter.h"
#include "animgoals/anim.h"

namespace bgwalk {
namespace {
int sWindow=-1, sButton=-1;
bool sUtility=true, sLatched=false;
constexpr int Size=36;
uint32_t sHoverSince=0;
void Tooltip(int,int,LgcyWidgetId*);

bool Scope() {
    return sUtility && gameSystems && party.GroupListGetLen()>0 &&
        gameSystems->GetTerrain().HasOriginalDoorInput();
}
bool Available() {
    return Scope() && (!bgscenecombat::GlobalCombatActive() || bgscenecombat::FreeSelection()) && !bgrolls::IsBusy() &&
        !tig->GetConsole().IsOpen() && !uiSystems->GetChar().IsVisible() &&
        !(uiDialog && (uiDialog->IsActive() || uiDialog->IsDialogEngaged())) &&
        !uiPicker.PickerActiveCheck();
}
bool Running() {
    return bgmovement::BaseRun(config.partyMovementMode,
        config.GetVanillaInt("always run")!=0, config.walkDistanceFt);
}

void Toggle() {
    if (!Available()) return;
    config.partyMovementMode=Running()?0:1;
    config.Save();
    auto &anim=gameSystems->GetAnim();
    for (uint32_t n=0;n<party.GroupListGetLen();++n)
        anim.RefreshPartyMovement(party.GroupListGetMemberN(n));
}

void Label(const std::string &text,int x,int y,int w) {
    ColorRect color(XMCOLOR(0.93f,0.83f,0.60f,1.0f)), shadow(XMCOLOR(0,0,0,0.8f));
    TigTextStyle style(&color);
    style.flags=8; style.shadowColor=&shadow;
    UiRenderer::PushFont(PredefinedFont::PRIORY_12);
    TigRect rect(x,y,w,70);
    UiRenderer::RenderText(text,rect,style);
    UiRenderer::PopFont();
}
void Render(int id) {
    auto b=uiManager->GetButton(id);
    if (!b || !Scope()) return;
    auto &s=tig->GetShapeRenderer2d();
    auto x=float(b->x), y=float(b->y);
    auto enabled=Available();
    auto gold=XMCOLOR(0.61f,0.48f,0.28f,enabled?1.0f:0.4f);
    s.DrawRectangle(x,y,Size,Size,XMCOLOR(0.07f,0.06f,0.04f,0.90f));
    s.DrawRectangleOutline({x+1,y+1},{x+Size-2,y+Size-2},gold);
    auto ink=enabled?XMCOLOR(0.55f,0.82f,0.88f,0.94f):XMCOLOR(0.4f,0.4f,0.4f,0.6f);
    // Two boot soles/heels drawn by the native UI renderer: no game asset.
    auto boot=[&](float bx,float by) {
        s.DrawRectangle(bx,by,5,8,ink);
        s.DrawRectangle(bx-1,by,7,3,ink);
        s.DrawRectangle(bx,by+10,5,3,ink);
    };
    boot(x+12,y+6); boot(x+22,y+(Running()?15:12));
    if (Running()) {
        for (int i=0;i<3;++i) s.DrawRectangle(x+5,y+18+i*4,5,1,gold);
    }
    TigMouseState mouse;mouseFuncs.GetState(&mouse);
    auto hovered=mouse.x>=b->x && mouse.x<b->x+Size && mouse.y>=b->y && mouse.y<b->y+Size;
    auto now=timeGetTime();
    if (!hovered) sHoverSince=0;
    else if (!sHoverSince) sHoverSince=now;
    else if (uint32_t(now-sHoverSince)>=550) Tooltip(0,0,nullptr);
}
void Tooltip(int, int, LgcyWidgetId*) {
    auto b=uiManager->GetButton(sButton);
    if (!b || !Scope()) return;
    auto &s=tig->GetShapeRenderer2d();
    auto x=b->x-230, y=b->y-74;
    s.DrawRectangle(float(x),float(y),264,68,XMCOLOR(0.05f,0.04f,0.03f,0.94f));
    s.DrawRectangleOutline({float(x),float(y)},{float(x+264),float(y+68)},XMCOLOR(0.61f,0.48f,0.28f,1.0f));
    // CP1251: "Бег / Ходьба", "Сейчас:", "Вне боя".
    auto text=std::string("\xC1\xE5\xE3 / \xD5\xEE\xE4\xFC\xE1\xE0 [Shift+R]\n\xD1\xE5\xE9\xF7\xE0\xF1: ")+
        (Running()?"\xC1\xE5\xE3":"\xD5\xEE\xE4\xFC\xE1\xE0")+"\n\xC2\xED\xE5 \xE1\xEE\xFF";
    Label(text,x+8,y+6,250);
}
}
void Initialize() {
    Shutdown();
    LgcyWindow window(0,0,Size,Size); window.flags=1;
    sWindow=uiManager->AddWindow(window);
    LgcyButton button("BG party movement",sWindow,0,0,Size,Size);
    button.render=Render; button.renderTooltip=nullptr;
    button.handleMessage=[](int,TigMsg *msg) {
        if (msg->type!=TigMsgType::WIDGET) return FALSE;
        // Native TranslateMouseMessage also queues widget releases.
        // HandleMouse owns this gesture; never toggle twice for one release.
        return TRUE;
    };
    button.SetDefaultSounds();
    sButton=uiManager->AddButton(button,sWindow);
    sUtility=true; sLatched=false; Resize();
}
void Shutdown() {
    if (sButton!=-1) uiManager->RemoveWidget(sButton);
    if (sWindow!=-1) uiManager->RemoveWidget(sWindow);
    sButton=sWindow=-1; sLatched=false;
}
void Resize() {
    if (sWindow==-1) return;
    auto canvas=uiManager->GetCanvasSize();
    auto w=uiManager->GetWindow(sWindow); auto b=uiManager->GetButton(sButton);
    if (w && b) { w->x=b->x=canvas.width-48; w->y=b->y=canvas.height-146; }
}
void Update() {
    if (sWindow!=-1) {
        auto window=uiManager->GetWindow(sWindow);
        auto hidden=!Scope();
        if (window && window->IsHidden()!=hidden) {
            uiManager->SetHidden(sWindow,hidden);
            sHoverSince=0;
            if (!hidden) uiManager->BringToFront(sWindow);
        }
    }
}
void SetUtilityVisible(bool visible) { sUtility=visible; Update(); }
bool HandleMouse(const TigMsg &msg) {
    static bool captured=false;
    if (msg.type!=TigMsgType::MOUSE) return false;
    auto b=sButton!=-1?uiManager->GetButton(sButton):nullptr;
    auto inside=b && Scope() && int(msg.arg1)>=b->x && int(msg.arg1)<b->x+Size &&
        int(msg.arg2)>=b->y && int(msg.arg2)<b->y+Size;
    if ((msg.arg4 & MSF_LMB_CLICK) && inside) captured=true;
    if (captured) {
        if (msg.arg4 & MSF_LMB_RELEASED) { captured=false; if (inside) Toggle(); }
        return true;
    }
    return inside && (msg.arg4 & (MSF_LMB_DOWN|MSF_LMB_RELEASED|MSF_RMB_CLICK|MSF_RMB_RELEASED));
}
bool HandleHotkey(const TigMsg &msg) {
    if (msg.type!=TigMsgType::KEYSTATECHANGE || msg.arg1!=0x13) return false; // DIK_R
    auto down=msg.arg2!=0;
    if (sLatched) { if (!down) sLatched=false; return true; }
    if (!down || !Available()) return false;
    auto &keys=infrastructure::gKeyboard;
    auto shift=keys.IsKeyPressed(VK_LSHIFT) || keys.IsKeyPressed(VK_RSHIFT);
    auto ctrl=keys.IsKeyPressed(VK_LCONTROL) || keys.IsKeyPressed(VK_RCONTROL);
    auto alt=keys.IsKeyPressed(VK_LMENU) || keys.IsKeyPressed(VK_RMENU);
    if (!shift || ctrl || alt) return false;
    sLatched=true; Toggle(); return true;
}
}
