// Included in ui_bgrolls.cpp after RecordOpenLock. Shares the native d20
// renderer, skill capture and UI lifetime; no separate render context.
struct ManualCheck {
    objHndl actor;
    SkillEnum skill=skill_search;
    int map=0,flags=0;
    std::string target,error;
    BonusList base;
    bgchecks::CheckState state;
    std::vector<CheckResource> resources;
    size_t scroll=0;
    std::function<bool()> valid;
    std::function<void(int,int,bool)> commit;
    std::function<void()> started;
};
std::unique_ptr<ManualCheck> sManual;
bool sTimeLocked=false;
constexpr int ManualW=600,ManualH=554,ResourceTop=342,ResourceStep=28,ResourceRows=3;

void LockCheckTime() {
    if (sTimeLocked) return;
    // Native time-advance lock is reference counted. These are the paired
    // push/pop entrypoints (verified in temple.dll), not a global pause toggle.
    if (temple::GetRef<int>(0x10AA83D8)>=30) throw std::runtime_error("too many time locks");
    gameSystems->GetTimeEvent().PushDisableAdvance();sTimeLocked=true;
}
void CloseManual() {
    sManual.reset();
    if (sTimeLocked) {sTimeLocked=false;temple::GetPointer<void()>(0x10060410)();}
    if (sWindow) {
        sWindow->SetSize({WndW,WndH});sWindow->SetCenterHorizontally(true);
        sWindow->SetCenterVertically(false);sWindow->SetY(70);
        if (sQueue.empty()) sWindow->Hide();
    }
}
bool ManualValid() {
    if (!sManual || !Enabled() || combatSys.isCombatActive()
        || maps.GetCurrentMapId()!=sManual->map || !objSystem->IsValidHandle(sManual->actor)
        || !party.IsInParty(sManual->actor) || critterSys.IsDeadOrUnconscious(sManual->actor)) return false;
    try {return !sManual->valid || sManual->valid();} catch (...) {return false;}
}
BonusList ManualBonuses() {
    auto bonuses=sManual->base;
    for (auto &r:sManual->resources) if (r.selected) bonuses.AddBonusWithDesc(r.value,r.type,0,r.label.c_str());
    return bonuses;
}
int ResourceGain(const CheckResource &source) {
    auto bonuses=sManual->base;
    for (auto &r:sManual->resources) if (r.selected && r.type!=source.type) bonuses.AddBonusWithDesc(r.value,r.type,0,r.label.c_str());
    auto before=bonuses.GetEffectiveBonusSum();
    bonuses.AddBonusWithDesc(source.value,source.type,0,source.label.c_str());
    return bonuses.GetEffectiveBonusSum()-before;
}
TigRect ManualArea() {
    auto canvas=uiManager->GetCanvasSize();
    return TigRect((canvas.width-ManualW)/2,(canvas.height-ManualH)/2,ManualW,ManualH);
}
void FinishManual() {
    if (!sManual || !sManual->state.Commit()) return;
    if (!ManualValid() || !ValidateCheckResources(sManual->actor,sManual->resources)) {
        logger->warn("BG check discarded: target or resource changed before commit");CloseManual();return;
    }
    // Move the callback out: a triggered trap or dialog may reset this UI.
    auto actor=sManual->actor;auto skill=sManual->skill;auto state=sManual->state;auto flags=sManual->flags;
    auto callback=sManual->commit;auto bonuses=ManualBonuses();auto resources=sManual->resources;
    CloseManual();
    try {
        CommitCheckResources(actor,resources);
        // Native one-shot effects (e.g. an already active Guidance) are consumed
        // here, after confirmation; all earlier dispatcher queries were read-only.
        dispatch.dispatch1ESkillLevel(actor,skill,nullptr,objHndl::null,flags);
        Dice die(1,20,0);
        histSys.RollHistoryAddType2SkillRoll(actor,die.ToPacked(),state.natural,state.dc,skill,&bonuses);
        auto special=skill==skill_open_lock || skill==skill_disable_device;
        auto success=state.Success(special);
        if (skill==skill_open_lock) RecordOpenLock(actor,state.natural==1);
        if (callback) callback(state.natural,state.natural+state.bonus,success);
        logger->info("BG manual check committed: actor={} skill={} die={} bonus={} dc={} success={}",
            actor.handle,int(skill),state.natural,state.bonus,state.dc,success);
    } catch (const std::exception &e) {logger->error("BG manual check commit: {}",e.what());}
}
void RollManual() {
    if (!sManual || sManual->state.phase!=bgchecks::Phase::Preparing) return;
    if (!ManualValid() || !ValidateCheckResources(sManual->actor,sManual->resources)) {
        sManual->error="Selected source is no longer available";return;
    }
    for (auto &r:sManual->resources) if (r.selected && ResourceGain(r)<=0) {
        sManual->error="This source does not increase the bonus";return;
    }
    auto bonuses=ManualBonuses();
    auto die=Dice(1,20,0).Roll(); // exactly one native RNG call, never a decorative reroll
    sManual->state.Roll(timeGetTime(),die,bonuses.GetEffectiveBonusSum(),sManual->state.dc);
    if (sManual->started) sManual->started();
}
bool DrawManual() {
    if (!sManual) return false;
    if (!ManualValid()) {CloseManual();return true;}
    sManual->state.Advance(timeGetTime());
    auto area=ManualArea();auto canvas=uiManager->GetCanvasSize();auto &shapes=tig->GetShapeRenderer2d();
    shapes.DrawRectangle(0,0,float(canvas.width),float(canvas.height),XMCOLOR(0.0f,0.0f,0.0f,0.60f));
    shapes.DrawRectangle(float(area.x),float(area.y),ManualW,ManualH,XMCOLOR(0.055f,0.05f,0.06f,0.98f));
    for (int inset:{1,5}) shapes.DrawRectangleOutline({float(area.x+inset),float(area.y+inset)},
        {float(area.x+ManualW-inset-1),float(area.y+ManualH-inset-1)},XMCOLOR(0.63f,0.50f,0.29f,1.0f));
    XMCOLOR gold(0.94f,0.78f,0.46f,1),white(0.91f,0.89f,0.84f,1),dim(0.65f,0.65f,0.65f,1);
    auto actor=description.getDisplayName(sManual->actor);auto name=skillSys.GetSkillName(sManual->skill);
    Text(fmt::format("{}: {}",actor,name),area.x+16,area.y+20,ManualW-32,gold,PredefinedFont::PRIORY_12,true);
    Text(sManual->target,area.x+16,area.y+44,ManualW-32,dim,PredefinedFont::PRIORY_12,true);
    Text("\xD1\xCB\xCE\xC6\xCD\xCE\xD1\xD2\xDC",area.x,area.y+74,ManualW,gold,PredefinedFont::PRIORY_12,true);
    Text(std::to_string(sManual->state.dc),area.x,area.y+94,ManualW,white,PredefinedFont::ARIAL_BOLD_24,true);
    auto phase=sManual->state.phase;bool rolling=phase==bgchecks::Phase::Rolling,result=phase==bgchecks::Phase::Result;
    float progress=rolling ? (timeGetTime()-sManual->state.started)/float(sManual->state.duration):1;
    auto ease=1-std::pow(1-(std::min)(1.0f,progress),3.0f);
    auto cx=area.x+ManualW/2.0f,cy=area.y+218.0f;
    auto landing=bgdice::Landing(sManual->state.natural ? sManual->state.natural:20);
    auto yaw=landing.yaw+(rolling ? (1-ease)*14:0),pitch=landing.pitch+(rolling ? (1-ease)*9:0);
    int frontNumber=20;
    auto face=DrawDie(cx+(rolling ? (1-ease)*65*sinf(progress*16):0),cy-(rolling ? fabsf(sinf(progress*19))*35*(1-ease):0),
        48,yaw,pitch,1,XMCOLOR(0.72f,0.60f,0.37f,1),&frontNumber);
    auto shown=phase==bgchecks::Phase::Preparing ? std::string("?"):std::to_string(frontNumber);
    Text(shown,int(face.x)-35,int(face.y)-14,70,white,PredefinedFont::ARIAL_BOLD_24,true);
    auto base=sManual->base.GetEffectiveBonusSum();auto bonus=ManualBonuses().GetEffectiveBonusSum();
    Text(fmt::format("\xC1\xEE\xED\xF3\xF1\x20\xED\xE0\xE2\xFB\xEA\xE0\x20\x7B\x3A\x2B\x7D\x20\x20\x20\xC4\xEE\xEF\xEE\xEB\xED\xE8\xF2\xE5\xEB\xFC\xED\xEE\x20\x7B\x3A\x2B\x7D\x20\x20\x20\xC8\xD2\xCE\xC3\xCE\x20\x7B\x3A\x2B\x7D",base,bonus-base,bonus),
        area.x+14,area.y+294,ManualW-28,gold,PredefinedFont::PRIORY_12,true);
    if (phase==bgchecks::Phase::Preparing) {
        Text("\xC4\xEE\xE1\xE0\xE2\xE8\xF2\xFC\x20\xE1\xEE\xED\xF3\xF1\x20\x28\xF1\xED\xE0\xF0\xFF\xE6\xE5\xED\xE8\xE5\x20\xE8\x20\xE0\xEA\xF2\xE8\xE2\xED\xFB\xE5\x20\xFD\xF4\xF4\xE5\xEA\xF2\xFB\x20\xF3\xE6\xE5\x20\xF3\xF7\xF2\xE5\xED\xFB\x29",area.x+16,area.y+320,ManualW-32,dim,PredefinedFont::PRIORY_12,true);
        if (sManual->resources.empty()) Text("\xCD\xE5\xF2\x20\xE4\xEE\xF1\xF2\xF3\xEF\xED\xFB\xF5\x20\xE4\xEE\xEF\xEE\xEB\xED\xE8\xF2\xE5\xEB\xFC\xED\xFB\xF5\x20\xE8\xF1\xF2\xEE\xF7\xED\xE8\xEA\xEE\xE2\x20\xF0\xFF\xE4\xEE\xEC",area.x+20,area.y+360,ManualW-40,dim,PredefinedFont::PRIORY_12,true);
        for (size_t row=0;row<ResourceRows && row+sManual->scroll<sManual->resources.size();++row) {
            auto &r=sManual->resources[row+sManual->scroll];auto y=area.y+ResourceTop+int(row)*ResourceStep;
            shapes.DrawRectangle(float(area.x+20),float(y),ManualW-40,ResourceStep-2,XMCOLOR(0.14f,0.13f,0.11f,1));
            auto label=r.label;if(label.size()>70) label=label.substr(0,67)+"...";
            Text(std::string(r.selected ? "[+] ":ResourceGain(r)>0 ? "[ ] ":"[0] ")+label,area.x+28,y+3,ManualW-56,r.selected ? gold:ResourceGain(r)>0 ? white:dim,PredefinedFont::PRIORY_12,false);
        }
        if (sManual->resources.size()>ResourceRows) Text(fmt::format("{} / {}",sManual->scroll+1,sManual->resources.size()),area.x+20,area.y+430,ManualW-40,dim,PredefinedFont::PRIORY_12,true);
    } else if (result) {
        auto state=sManual->state;auto success=state.Success(sManual->skill==skill_open_lock || sManual->skill==skill_disable_device);
        Text(fmt::format("\x64\x32\x30\x3A\x20\x7B\x7D\x20\x7B\x3A\x2B\x7D\x20\x3D\x20\x7B\x7D\x20\x20\x20\xEF\xF0\xEE\xF2\xE8\xE2\x20\xF1\xEB\xEE\xE6\xED\xEE\xF1\xF2\xE8\x20\x7B\x7D",state.natural,state.bonus,state.natural+state.bonus,state.dc),
            area.x+16,area.y+340,ManualW-32,white,PredefinedFont::PRIORY_12,true);
        auto critical=state.natural==1 && (sManual->skill==skill_open_lock || sManual->skill==skill_disable_device);
        Text(critical ? "\xCA\xD0\xC8\xD2\xC8\xD7\xC5\xD1\xCA\xC8\xC9\x20\xCF\xD0\xCE\xC2\xC0\xCB" : success ? "\xD3\xD1\xCF\xC5\xD5":"\xCF\xD0\xCE\xC2\xC0\xCB",area.x+16,area.y+380,ManualW-32,
            success ? XMCOLOR(0.3f,0.85f,0.4f,1):XMCOLOR(0.95f,0.3f,0.25f,1),PredefinedFont::ARIAL_BOLD_24,true);
    } else Text("\xCF\xF0\xEE\xE2\xE5\xF0\xEA\xE0\x20\xE2\xFB\xEF\xEE\xEB\xED\xFF\xE5\xF2\xF1\xFF\x2E\x2E\x2E",area.x+16,area.y+360,ManualW-32,dim,PredefinedFont::PRIORY_12,true);
    if (!sManual->error.empty()) Text(sManual->error,area.x+16,area.y+444,ManualW-32,white,PredefinedFont::PRIORY_12,true);
    if (phase==bgchecks::Phase::Preparing) {
        shapes.DrawRectangle(float(area.x+20),float(area.y+478),120,38,XMCOLOR(0.16f,0.15f,0.13f,1));
        Text("\xCE\xF2\xEC\xE5\xED\xE0",area.x+20,area.y+488,120,dim,PredefinedFont::PRIORY_12,true);
    }
    if (!rolling) {
        shapes.DrawRectangle(float(area.x+165),float(area.y+478),270,38,XMCOLOR(0.27f,0.22f,0.13f,1));
        Text(result ? "\xCF\xD0\xCE\xC4\xCE\xCB\xC6\xC8\xD2\xDC":"\xC1\xD0\xCE\xD1\xC8\xD2\xDC\x20\xCA\xD3\xC1\xC8\xCA",area.x+165,area.y+488,270,gold,PredefinedFont::PRIORY_12,true);
    }
    Text(phase==bgchecks::Phase::Preparing ? "\x45\x73\x63\x20\x97\x20\xEE\xF2\xEC\xE5\xED\xE0\x2E\x20\xCA\xEE\xEB\xE5\xF1\xEE\x20\x97\x20\xF1\xEF\xE8\xF1\xEE\xEA\x20\xE1\xEE\xED\xF3\xF1\xEE\xE2\x2E"
        : "\xC4\xE5\xE9\xF1\xF2\xE2\xE8\xE5\x20\xE8\x20\xF0\xE0\xF1\xF5\xEE\xE4\x20\xF0\xE5\xF1\xF3\xF0\xF1\xEE\xE2\x20\x97\x20\xEF\xEE\xF1\xEB\xE5\x20\xAB\xCF\xF0\xEE\xE4\xEE\xEB\xE6\xE8\xF2\xFC\xBB",area.x+16,area.y+527,ManualW-32,dim,PredefinedFont::PRIORY_12,true);
    return true;
}
