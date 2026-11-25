#include "stdafx.h"
#include "ui_char_workspace.h"
#include "charworkspace_quote.h"

#include <graphics/device.h>
#include <graphics/shaperenderer2d.h>
#include <graphics/textengine.h>
#include <infrastructure/stringutil.h>

#include "combat.h"
#include "critter.h"
#include "d20.h"
#include "gamesystems/objects/objsystem.h"
#include "inventory.h"
#include "location.h"
#include "obj.h"
#include "party.h"
#include "tig/tig_msg.h"
#include "tig/tig_startup.h"
#include "ui_assets.h"
#include "ui_bgpanel.h"
#include "ui_bgrolls.h"
#include "ui_char.h"
#include "ui_dialog.h"
#include "ui_render.h"
#include "ui_systems.h"

namespace charworkspace {
namespace {

constexpr int Width = 864, Height = 572, Rows = 8, IdentifyPrice = 10000;
constexpr uint32_t Paper = 0xFF152027, Well = 0xFF19262E, Row = 0xFF203039;
constexpr uint32_t Ink = 0xFFEDE8D7, Muted = 0xFFA9BBC3, Gold = 0xFFE3BA70;
constexpr uint32_t Edge = 0xFF394E56, Selected = 0xFF334950;
constexpr char EnableResource[] = "mes/bg2toee_character_workspace.mes";

enum class Filter { All, Gear, Consumable, Other };
enum class Order { Slot, Name, Weight };
enum class Action { None, Buy, Sell, Identify, Drop, Equip, Give };

struct Box {
	int x, y, w, h;
	bool operator==(const Box &other) const {
		return x == other.x && y == other.y && w == other.w && h == other.h;
	}
};

struct View {
	objHndl nativeActor{}, owner{}, vendor{}, other{};
	UiCharDisplayType mode = UiCharDisplayType::Hidden;
	bool Trading() const { return mode == UiCharDisplayType::Bartering; }
	bool HasOther() const { return other != objHndl::null; }
};

struct Item {
	objHndl object;
	std::wstring name;
	int slot, quantity, weight;
};

struct Control {
	Box box;
	std::function<void()> click;
};

View sView;
objHndl sItem{};
Action sAction = Action::None;
Quote sQuote{};
bool sNative = false, sEnabled = false, sReadEnable = false;
bool sPressed = false;
Box sPressBox{};
int sMouseX = -1, sMouseY = -1;
int sPage = 0, sOtherPage = 0;
Filter sFilter = Filter::All;
Order sOrder = Order::Slot;
std::wstring sStatus;
std::vector<Control> sControls;
float sScale = 1;
int sLeft = 0, sTop = 0;

bool Valid(objHndl obj) {
	return obj && objSystem->IsValidHandle(obj);
}

bool Alive(objHndl obj) {
	return Valid(obj) && objSystem->GetObject(obj)->IsCritter() && !critterSys.IsDeadOrUnconscious(obj);
}

std::wstring Name(objHndl obj) {
	return Valid(obj) ? local_to_ucs2(objects.GetDisplayName(obj, sView.owner)) : L"";
}

UiChar &Native() { return uiSystems->GetChar(); }

void ClearSelection() {
	sItem = {};
	sAction = Action::None;
	sStatus.clear();
}

bool SyncView() {
	if (!uiSystems || !objSystem || !vfs) return false;
	auto &native = Native();
	auto actor = native.GetCritter();
	// This is the stock visibility query also used by the inventory hotkey.
	bool visible = actor && temple::GetRef<int(__cdecl)()>(0x10143820)();
	if (!visible) {
		if (sView.nativeActor) Reset();
		return false;
	}
	if (!sReadEnable) {
		sEnabled = vfs->FileExists(EnableResource);
		sReadEnable = true;
	}
	if (!sEnabled || sNative || bgpanel::IsVisible() || bgrolls::IsBusy()
		|| (uiDialog && uiDialog->IsActive()) || combatSys.isCombatActive()
		|| !Alive(actor) || !party.IsInParty(actor)) return false;

	auto mode = native.GetDisplayType();
	if (mode != UiCharDisplayType::Hidden && mode != UiCharDisplayType::Bartering
		&& mode != UiCharDisplayType::Looting) return false;

	objHndl vendor{}, other{};
	if (mode != UiCharDisplayType::Hidden) {
		if (!native.IsLootingActive()) return false;
		other = native.GetLootedObject();
		if (mode == UiCharDisplayType::Bartering) {
			vendor = temple::GetRef<objHndl>(0x10BE6EC8);
			if (!Valid(vendor)) vendor = other;
			if (!Alive(vendor)) return false;
			if (auto stock = inventory.GetSubstituteInventory(vendor); Valid(stock)) other = stock;
		}
		if (!Valid(other) || other == actor) return false;
		auto holder = objSystem->GetObject(other);
		if (!holder->IsCritter() && !holder->IsContainer()) return false;
	}
	if (sView.nativeActor != actor || sView.mode != mode || sView.other != other || sView.vendor != vendor) {
		sView = { actor, actor, vendor, other, mode };
		logger->info("Character workspace opened, mode {}", static_cast<uint32_t>(mode));
		sPage = sOtherPage = 0;
		sPressed = false;
		ClearSelection();
	}
	if (!Alive(sView.owner) || !party.IsInParty(sView.owner)) {
		sView.owner = actor;
		ClearSelection();
	}
	return true;
}

TigRect Pixel(Box r) {
	const auto x = int(r.x*sScale), y = int(r.y*sScale);
	return { sLeft+x, sTop+y, std::max(1, int((r.x+r.w)*sScale)-x),
		std::max(1, int((r.y+r.h)*sScale)-y) };
}

bool Hovered(Box r) {
	const auto rect = Pixel(r);
	return sMouseX >= rect.x && sMouseY >= rect.y
		&& sMouseX < rect.x + rect.width && sMouseY < rect.y + rect.height;
}

void Fill(Box r, uint32_t color) {
	auto p = Pixel(r);
	tig->GetShapeRenderer2d().DrawRectangle(float(p.x), float(p.y), float(p.width), float(p.height), color);
}

void Text(Box r, const std::wstring &text, int size = 12, uint32_t color = Ink,
	gfx::TextAlign align = gfx::TextAlign::Left, bool bold = false) {
	gfx::TextStyle style;
	style.fontFace = "Arial";
	style.pointSize = std::max(8.0f, size*sScale);
	style.foreground.primaryColor = XMCOLOR(color);
	style.align = align;
	style.paragraphAlign = gfx::ParagraphAlign::Center;
	style.bold = bold;
	style.trim = true;
	tig->GetRenderingDevice().GetTextEngine().RenderText(Pixel(r), style, text);
}

void Rule(int x, int y, int w) { Fill({ x,y,w,1 }, Edge); }

void Button(Box r, const std::wstring &text, std::function<void()> click, bool selected = false) {
	const auto hover = click && Hovered(r);
	Fill(r, hover || selected ? Selected : Row);
	if (selected || hover) Fill({ r.x,r.y+r.h-2,r.w,2 }, Gold);
	Text(r, text, 12, click ? (selected ? Gold : Ink) : Muted, gfx::TextAlign::Center);
	if (click) sControls.push_back({ r, std::move(click) });
}

void Icon(objHndl obj, Box box) {
	if (!Valid(obj)) return;
	auto id = static_cast<UiGenericAsset>(objSystem->GetObject(obj)->GetInt32(obj_f_item_inv_aid));
	int tex = 0;
	if (uiAssets->GetAsset(UiAssetType::Inventory, id, tex) && tex)
		UiRenderer::DrawTexture(tex, Pixel(box), TigRect(0,0,64,64));
}

std::wstring Coins(int cp) {
	return std::to_wstring(cp/100) + L" зм. " + std::to_wstring((cp%100)/10)
		+ L" см. " + std::to_wstring(cp%10) + L" мм.";
}

Filter Category(objHndl obj) {
	auto item = objSystem->GetObject(obj);
	switch (item->type) {
	case obj_t_weapon: case obj_t_armor: case obj_t_ammo: return Filter::Gear;
	case obj_t_food: case obj_t_scroll: return Filter::Consumable;
	case obj_t_generic:
		if (item->GetSpellArray(obj_f_item_spell_idx).GetSize()) return Filter::Consumable;
		break;
	default: break;
	}
	return Filter::Other;
}

int Quantity(objHndl obj) { return std::max(1, inventory.GetQuantity(obj)); }

bool Worn(objHndl item) { return inventory.IsInvIdxWorn(inventory.GetInventoryLocation(item)); }

bool Accessible(objHndl item, objHndl holder) {
	if (!Valid(holder) || !Valid(item) || !objSystem->GetObject(item)->IsItem()
		|| inventory.GetParent(item) != holder) return false;
	auto holderObject = objSystem->GetObject(holder);
	if (holderObject->IsCritter() && d20Sys.d20Query(holder, DK_QUE_Polymorphed)
		&& !inventory.ItemAccessibleDuringPolymorph(item)) return false;
	if (sView.Trading() && objSystem->GetObject(item)->type == obj_t_money) return false;
	if (holder == sView.other) {
		auto flags = inventory.GetItemFlags(item);
		if ((flags & OIF_NO_LOOT) || (sView.Trading() && Worn(item))) return false;
		if (objSystem->GetObject(holder)->IsCritter() && Worn(item) && (flags & OIF_NO_DROP)) return false;
	}
	return true;
}

std::vector<Item> Items(objHndl holder, bool excludeWorn) {
	std::vector<Item> result;
	for (auto item : inventory.GetInventory(holder)) {
		if (!Accessible(item, holder) || (excludeWorn && Worn(item))
			|| (sFilter != Filter::All && Category(item) != sFilter)) continue;
		result.push_back({ item, Name(item), inventory.GetInventoryLocation(item),
			Quantity(item), inventory.ItemWeight(item) });
	}
	std::stable_sort(result.begin(), result.end(), [](const Item &a, const Item &b) {
		if (sOrder == Order::Name && a.name != b.name) return a.name < b.name;
		if (sOrder == Order::Weight && a.weight != b.weight) return a.weight > b.weight;
		return a.slot < b.slot;
	});
	return result;
}

int TradePrice(objHndl item, bool buy) {
	auto appraiser = party.PartyMemberWithHighestSkill(SkillEnum::skill_appraise);
	if (!Valid(appraiser) || !Alive(sView.vendor)) return 0;
	auto price = buy
		? inventory.GetAppraisedTransactionSum(item, sView.vendor, appraiser, SkillEnum::skill_appraise)
		: inventory.GetAppraisedWorth(item, appraiser, sView.vendor, SkillEnum::skill_appraise);
	return StackPrice(price, Quantity(item));
}

void Select(objHndl item) {
	ClearSelection();
	sItem = item;
}

void Close() {
	Native().Hide();
	Reset();
}

bool TransferResult(ItemErrorCode error, const wchar_t *success) {
	if (error == IEC_OK) {
		ClearSelection();
		sStatus = success;
		return true;
	}
	sStatus = local_to_ucs2(inventory.GetItemErrorString(error));
	return false;
}

void Move(objHndl from, objHndl to, int slot, int flags, const wchar_t *success) {
	auto item = sItem;
	if (!Accessible(item, from) || !Valid(to)) return;
	TransferResult(inventory.TransferWithFlags(item, to, slot, flags, objHndl::null), success);
}

void BeginTrade(bool buy) {
	if (!Accessible(sItem, buy ? sView.other : sView.owner)) return;
	auto price = TradePrice(sItem, buy);
	if (price <= 0) {
		sStatus = L"Торговец не предлагает цену за этот предмет.";
		return;
	}
	sQuote = { price, Quantity(sItem) };
	sAction = buy ? Action::Buy : Action::Sell;
	sStatus.clear();
}

void Confirm() {
	const auto action = sAction;
	auto item = sItem;
	sAction = Action::None;
	if (action == Action::Drop && Accessible(item, sView.owner)) {
		if (inventory.ItemDrop(item)) {
			ClearSelection();
			sStatus = L"Предмет оставлен на земле.";
		} else {
			sStatus = L"Этот предмет нельзя выбросить.";
		}
		return;
	}
	if (!sView.Trading()) return;
	if (action == Action::Identify) {
		if (!Accessible(item,sView.owner) || inventory.IsIdentified(item)) return;
		if (party.GetMoney() < IdentifyPrice) {
			sStatus = L"У группы недостаточно денег.";
			return;
		}
		objSystem->GetObject(item)->SetItemFlag(OIF_IDENTIFIED, true);
		party.DebitMoney(0, IdentifyPrice/100, 0, 0);
		sStatus = L"Предмет опознан.";
		return;
	}
	const bool buy = action == Action::Buy;
	if (!buy && action != Action::Sell) return;
	const auto from = buy ? sView.other : sView.owner;
	const auto to = buy ? sView.owner : sView.other;
	if (!Accessible(item, from) || !Valid(to)) return;
	if (Worn(item)) {
		sStatus = L"Сначала снимите предмет.";
		return;
	}
	switch (ValidateQuote(sQuote, { TradePrice(item, buy), Quantity(item) }, party.GetMoney(), buy)) {
	case QuoteResult::Changed:
		sStatus = L"Цена или количество изменились. Проверьте новый итог.";
		return;
	case QuoteResult::InsufficientFunds:
		sStatus = L"У группы недостаточно денег.";
		return;
	case QuoteResult::WalletFull:
		sStatus = L"Кошелёк переполнен.";
		return;
	case QuoteResult::Ready:
		break;
	}
	int p = 0, g = 0, s = 0, c = 0;
	inventory.MoneyToCoins(sQuote.amount, &p, &g, &s, &c);
	const auto flags = buy ? IIF_4 : IIF_4 | IIF_Use_Max_Idx_200;
	if (!TransferResult(inventory.TransferWithFlags(item, to, -1, flags, objHndl::null),
		buy ? L"Покупка завершена." : L"Продажа завершена.")) return;
	if (buy) party.DebitMoney(p,g,s,c);
	else party.MoneyAdj(p,g,s,c);
}

void Use() {
	auto owner = sView.owner;
	auto item = sItem;
	if (!Accessible(item, owner) || !inventory.IsIdentified(item)
		|| objSystem->GetObject(item)->type != obj_t_generic
		|| Category(item) != Filter::Consumable) return;
	auto result = combatSys.UseItem(owner, item, owner);
	if (result == 0) Close();
	else sStatus = L"Предмет сейчас нельзя применить.";
}

void PartyMember(objHndl who) {
	if (!Alive(who) || !party.IsInParty(who)) return;
	if (sAction == Action::Give && who != sView.owner) {
		if (locSys.DistanceToObj_NonNegative(sView.owner, who) > 10) {
			sStatus = L"Подведите этого спутника ближе.";
			return;
		}
		Move(sView.owner, who, -1, IIF_4, L"Предмет передан.");
		return;
	}
	sView.owner = who;
	sPage = 0;
	ClearSelection();
}

void Header() {
	Fill({0,0,Width,Height}, Paper);
	Fill({0,0,Width,3}, Gold);
	Text({18,13,188,30}, sView.Trading() ? L"Торговля" : sView.HasOther() ? L"Добыча" : L"Снаряжение",
		23, Gold, gfx::TextAlign::Left, true);
	Text({215,12,373,33}, Name(sView.owner), 20);
	Text({573,16,168,26}, Coins(party.GetMoney()), 12, Gold, gfx::TextAlign::Right);
	Button({754,12,92,35}, L"Закрыть", Close);
	Rule(18,56,828);
	const auto count = std::min<uint32_t>(party.GroupListGetLen(), PARTY_SIZE_MAX);
	const auto chip = std::max(85, 828 / std::max(1, int(count)));
	for (uint32_t i = 0; i < count; ++i) {
		auto who = party.GroupListGetMemberN(i);
		Button({18 + int(i)*chip,65,chip-5,29}, Name(who),
			Alive(who) ? std::function<void()>([who] { PartyMember(who); }) : nullptr, who == sView.owner);
	}
}

void Filters() {
	const wchar_t *names[] = {L"Все", L"Экипировка", L"Расходники", L"Прочее"};
	for (int i = 0; i < 4; ++i) {
		const auto filter = Filter(i);
		Button({18+i*103,105,98,27}, names[i], [filter] {
			sFilter = filter;
			sPage = sOtherPage = 0;
		}, sFilter == filter);
	}
	const wchar_t *sorts[] = {L"В порядке ячеек", L"По названию", L"Сначала тяжёлые"};
	Button({452,105,182,27}, sorts[static_cast<int>(sOrder)], [] {
		sOrder = Order((static_cast<int>(sOrder)+1)%3);
		sPage = sOtherPage = 0;
	});
	Button({643,105,203,27}, L"Книга правил и умений", [] { sNative = true; ClearSelection(); });
}

void List(Box pane, const std::wstring &label, objHndl holder, int &page, bool excludeWorn) {
	auto items = Items(holder, excludeWorn);
	const int pages = std::max(1, (static_cast<int>(items.size()) + Rows - 1)/Rows);
	page = std::clamp(page, 0, pages-1);
	Fill(pane, Well);
	Text({pane.x+10,pane.y+3,pane.w-20,27}, label, 15, Gold, gfx::TextAlign::Left, true);
	Rule(pane.x+10,pane.y+30,pane.w-20);
	for (int i = 0; i < Rows && page*Rows+i < static_cast<int>(items.size()); ++i) {
		const auto &item = items[page*Rows+i];
		const Box r{ pane.x+6,pane.y+37+i*34,pane.w-12,31 };
		bool chosen = sItem == item.object;
		bool hovered = Hovered(r);
		Fill(r, chosen || hovered ? Selected : Row);
		if (chosen) Fill({r.x,r.y,3,r.h}, Gold);
		Icon(item.object, {r.x+6,r.y+2,27,27});
		auto name = item.name;
		if (Worn(item.object)) name += L" [надето]";
		Text({r.x+41,r.y,r.w-90,r.h}, name, 12);
		Text({r.x+r.w-49,r.y,42,r.h}, L"×" + std::to_wstring(item.quantity), 11, Muted, gfx::TextAlign::Right);
		const auto handle = item.object;
		sControls.push_back({r, [handle] { Select(handle); }});
	}
	if (items.empty()) Text({pane.x+11,pane.y+43,pane.w-22,34}, L"Список пуст", 12, Muted);
	const int y = pane.y + 313;
	auto *pageNumber = &page;
	Button({pane.x+8,y,31,25}, L"‹", page > 0 ? std::function<void()>([pageNumber] { --*pageNumber; }) : nullptr);
	Button({pane.x+pane.w-39,y,31,25}, L"›", page+1 < pages ? std::function<void()>([pageNumber] { ++*pageNumber; }) : nullptr);
	Text({pane.x+42,y,pane.w-84,25}, std::to_wstring(page+1) + L" / " + std::to_wstring(pages),
		11, Muted, gfx::TextAlign::Center);
}

void Equipment() {
	const wchar_t *slots[] = { L"Голова", L"Амулет", L"Перчатки", L"Правая рука",
		L"Левая рука", L"Доспех", L"Кольцо", L"Кольцо", L"Обувь", L"Боеприпасы",
		L"Плащ", L"Щит", L"Роба", L"Наручи", L"Инструмент", L"Отмычки" };
	const EquipSlot order[] = { Helmet, Cloak, Armor, Robes, Gloves, Bracers, Boots, Necklace,
		WeaponPrimary, WeaponSecondary, Ammo, Shield, RingPrimary, RingSecondary, BardicItem, Lockpicks };
	Fill({18,145,240,343}, Well);
	Text({29,148,216,27}, L"На персонаже", 15, Gold, gfx::TextAlign::Left, true);
	for (int i = 0; i < 16; ++i) {
		auto slot = order[i];
		auto item = inventory.ItemWornAt(sView.owner, slot);
		Box r{ 25+(i%2)*115,181+(i/2)*36,111,32 };
		bool target = sAction == Action::Equip;
		Fill(r, Hovered(r) || (item && item == sItem) ? Selected : Row);
		if (target) Fill({r.x,r.y,2,r.h}, Gold);
		Icon(item, {r.x+3,r.y+3,26,26});
		Text({r.x+32,r.y,r.w-34,r.h}, slots[slot], 10, item ? Ink : Muted);
		sControls.push_back({r, [item, slot] {
			if (sAction == Action::Equip) {
				Move(sView.owner, sView.owner, INVENTORY_WORN_IDX_START + slot,
					IIF_Allow_Swap, L"Предмет надет.");
			} else {
				Select(item);
			}
		}});
	}
}

void Stats() {
	auto pc = sView.owner;
	const Stat stats[] = {stat_strength,stat_dexterity,stat_constitution,stat_intelligence,
		stat_wisdom,stat_charisma,stat_save_fortitude,stat_save_reflexes,stat_save_willpower};
	const wchar_t *labels[] = {L"Сила", L"Ловкость", L"Телосложение", L"Интеллект",
		L"Мудрость", L"Харизма", L"Стойкость", L"Реакция", L"Воля"};
	Text({628,183,205,35}, L"Уровень " + std::to_wstring(objects.StatLevelGet(pc, stat_level))
		+ L"  ·  КБ " + std::to_wstring(objects.StatLevelGet(pc, stat_ac)), 14, Gold);
	Text({628,217,205,28}, L"Здоровье  " + std::to_wstring(objects.GetHPCur(pc))
		+ L" / " + std::to_wstring(objects.StatLevelGet(pc,stat_hp_max)), 13);
	for (int i = 0; i < 9; ++i) {
		const int y = 253+i*24;
		Rule(628,y,204);
		Text({628,y,147,24}, labels[i], 11, Muted);
		Text({777,y,54,24}, std::to_wstring(objects.StatLevelGet(pc,stats[i])), 12, Ink, gfx::TextAlign::Right);
	}
}

bool CanUse(objHndl item) {
	return objSystem->GetObject(item)->type == obj_t_generic
		&& Category(item) == Filter::Consumable && inventory.IsIdentified(item);
}

void ItemDetails() {
	Fill({616,145,230,343}, Well);
	if (!Accessible(sItem,sView.owner)) {
		Text({628,148,204,27}, L"Характеристики", 15, Gold, gfx::TextAlign::Left, true);
		Stats();
		return;
	}
	Text({628,148,204,27}, L"Карточка предмета", 15, Gold, gfx::TextAlign::Left, true);
	auto item = sItem;
	auto worn = Worn(item);
	Icon(item, {628,185,48,48});
	Text({687,181,146,57}, Name(item), 14, Ink, gfx::TextAlign::Left, true);
	Rule(628,248,205);
	Text({628,255,205,25}, L"Количество  " + std::to_wstring(Quantity(item)), 12);
	Text({628,280,205,25}, L"Вес  " + std::to_wstring(inventory.ItemWeight(item)) + L" фн.", 12);
	Text({628,306,205,23}, worn ? L"На персонаже" : L"В рюкзаке", 11, Muted);
	if (sAction == Action::Drop) {
		Button({628,343,205,33}, L"Оставить весь предмет", Confirm);
		Button({628,386,205,33}, L"Отмена", [] { sAction = Action::None; });
		return;
	}
	Button({628,343,97,33}, worn ? L"Снять" : L"Надеть", [worn] {
		if (worn) Move(sView.owner,sView.owner,-1,IIF_4,L"Предмет снят.");
		else { sAction = Action::Equip; sStatus = L"Выберите ячейку экипировки."; }
	});
	Button({734,343,99,33}, L"Применить", CanUse(item) ? std::function<void()>(Use) : nullptr);
	Button({628,385,97,33}, L"Передать", [] {
		sAction = Action::Give;
		sStatus = L"Выберите спутника в верхней строке.";
	});
	Button({734,385,99,33}, L"Выбросить", [] { sAction = Action::Drop; sStatus.clear(); });
	if (sAction == Action::Equip || sAction == Action::Give)
		Button({628,427,205,33}, L"Отменить выбор", [] { sAction = Action::None; sStatus.clear(); });
}

void TakeAll() {
	const auto holder = sView.other;
	int taken = 0;
	for (auto item : inventory.GetInventory(holder)) {
		if (!Accessible(item,holder)) continue;
		const auto result = inventory.TransferWithFlags(item, sView.owner, -1, IIF_4, objHndl::null);
		if (result == IEC_OK) ++taken;
		else if (result == IEC_No_Room_For_Item) {
			ClearSelection();
			sStatus = L"Рюкзак заполнен. Перенесено позиций: " + std::to_wstring(taken);
			return;
		}
	}
	ClearSelection();
	sStatus = L"Перенесено позиций: " + std::to_wstring(taken);
}

void ExchangeDetails() {
	Fill({18,499,828,45}, Well);
	if (sAction == Action::Buy || sAction == Action::Sell || sAction == Action::Identify) {
		std::wstring question;
		if (sAction == Action::Identify) question = L"Опознать предмет за " + Coins(IdentifyPrice) + L"?";
		else question = std::wstring(sAction == Action::Buy ? L"Заплатить " : L"Получить ")
			+ Coins(sQuote.amount) + L" за " + std::to_wstring(sQuote.quantity) + L" шт.?";
		Text({29,500,514,43}, question, 13, Gold);
		Button({558,505,142,32}, L"Подтвердить", Confirm);
		Button({712,505,122,32}, L"Отмена", [] { sAction = Action::None; });
		return;
	}
	const bool theirs = Accessible(sItem,sView.other);
	const bool ours = Accessible(sItem,sView.owner);
	if (!sView.Trading()) Button({688,505,146,32}, L"Забрать всё", TakeAll);
	if (!ours && !theirs) {
		Text({29,500,650,43}, sView.Trading() ? L"Выберите предмет для покупки или продажи."
			: L"Выберите предмет из найденного.", 12, Muted);
		return;
	}
	Text({29,500,303,43}, Name(sItem), 13);
	if (sView.Trading()) {
		const auto price = TradePrice(sItem,theirs);
		Text({339,500,204,43}, price ? Coins(price) : L"Цена отсутствует", 13, Gold);
		if (ours && !inventory.IsIdentified(sItem)) {
			Button({554,505,122,32}, L"Опознать", [] { sAction = Action::Identify; sStatus.clear(); });
		}
		Button({688,505,146,32}, theirs ? L"Купить" : L"Продать",
			price && !Worn(sItem) ? std::function<void()>([theirs] { BeginTrade(theirs); }) : nullptr);
	} else if (theirs) {
		Button({528,505,146,32}, L"Забрать", [] { Move(sView.other,sView.owner,-1,IIF_4,L"Предмет получен."); });
	} else if (!Worn(sItem)) {
		Button({528,505,146,32}, L"Переложить", [] { Move(sView.owner,sView.other,-1,IIF_4,L"Предмет переложен."); });
	}
}

} // namespace

void Reset() {
	sView = {};
	sNative = false;
	sPressed = false;
	sPage = sOtherPage = 0;
	sControls.clear();
	ClearSelection();
}

bool Render() {
	if (!SyncView()) return false;
	auto canvas = uiManager->GetCanvasSize();
	if (canvas.width <= 0 || canvas.height <= 0) return false;
	sScale = std::min({ float(canvas.width)/Width, float(canvas.height)/Height, 1.8f });
	sLeft = (canvas.width-int(Width*sScale))/2;
	sTop = (canvas.height-int(Height*sScale))/2;
	sControls.clear();
	tig->GetShapeRenderer2d().DrawRectangle(0,0,float(canvas.width),float(canvas.height),0xA0081116);
	Header();
	Filters();
	if (sView.HasOther()) {
		auto title = Name(sView.Trading() ? sView.vendor : sView.other);
		List({18,145,409,343}, title, sView.other, sOtherPage, false);
		List({437,145,409,343}, L"Вещи персонажа", sView.owner, sPage, false);
		ExchangeDetails();
	} else {
		Equipment();
		List({270,145,334,343}, L"Рюкзак", sView.owner, sPage, true);
		ItemDetails();
		int weight = 0;
		for (auto item : inventory.GetInventory(sView.owner))
			if (Valid(item)) weight += inventory.ItemWeight(item);
		Text({19,500,238,31}, L"Вес снаряжения  " + std::to_wstring(weight) + L" фн.", 12, Muted);
	}
	Text({21,547,820,21}, sStatus, 12, Gold);
	return true;
}

bool HandleInput(const TigMsg &msg) {
	if (!SyncView()) return false;
	switch (msg.type) {
	case TigMsgType::KEYSTATECHANGE:
		if (!msg.arg2 && (msg.arg1 == 0x01 || msg.arg1 == 0x17)) {
			if (sAction != Action::None) { sAction = Action::None; sStatus.clear(); }
			else Close();
		}
		return true;
	case TigMsgType::MOUSE: {
		sMouseX = static_cast<int>(msg.arg1);
		sMouseY = static_cast<int>(msg.arg2);
		if (msg.arg4 & MSF_SCROLLWHEEL_CHANGE) {
			const Box theirs{18,145,409,343};
			const Box ours = sView.HasOther() ? Box{437,145,409,343} : Box{270,145,334,343};
			const int delta = static_cast<int>(msg.arg3) > 0 ? -1 : 1;
			if (sView.HasOther() && Hovered(theirs)) sOtherPage += delta;
			else if (Hovered(ours)) sPage += delta;
		}
		auto hovered = std::find_if(sControls.begin(), sControls.end(), [](const Control &control) {
			return Hovered(control.box);
		});
		if (msg.arg4 & MSF_LMB_CLICK) {
			sPressed = hovered != sControls.end();
			if (sPressed) sPressBox = hovered->box;
		}
		if (msg.arg4 & MSF_LMB_RELEASED) {
			auto click = sPressed && hovered != sControls.end() && hovered->box == sPressBox
				? hovered->click : std::function<void()>{};
			sPressed = false;
			if (click) click();
		}
		return true;
	}
	case TigMsgType::WIDGET: case TigMsgType::CHAR: case TigMsgType::KEYDOWN:
		return true;
	default: return false;
	}
}

} // namespace charworkspace
