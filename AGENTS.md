# AGENTS.md — форк Temple+ для bg2toee

## Верхний этаж таверны — 7 ноября, актуально

- fix/bg-upper-floor-navigation, draft PR25 поверх24, code6575409. Пользователь
  подтвердил:clearance-only converterPR48 не исправил недоступные комнаты.
- pathfinding.cpp:BgUpperFloorCell проверяетregisteredscene5191 иsector8,12,
  valid/nonzerocentre clearance. Оба PathDestIsClear не отвергают доступный
  центр из-за peripheral sectorhits;собираютвсеraycasthits и сохраняют full
  actor radius collision. Оба A* staticclearance используют centrepolicy;
  dynamicdoorblockers/proximityradius не меняются. Другие карты сохраняют
  буквальное native comparison. bginteriornavigation.h содержит scope/policy,
  standalone positive/negative tests включены в registry suite.
- BuildCI42336713342 success,0.2.120,job128295158206:registry/policy,
  6savehooks,13images OK. Artifact13559774026,23,574,131B,GitHub ZIP SHA256
  aafef285a85a8c226f34ff83674d7ffaf13c31081a96575e9f9c64dff83aa2b7.
  https://github.com/DnDuncan/TemplePlus/actions/runs/42336713342/artifacts/13559774026
  Не выдавать старую0.2.117 как содержащуюфикс;native gameplay ещё не проверен.
  УстановкаEXE только пользователем в новую F:папку. Игру/CUA не запускать.
- Converter PR49 fix/iwd2-equipped-icon-alpha, ef383f3:neutralRGB приalpha0
  исправляет hiddengreenkey23TGA. Installed receipt14 journal
  interaction-fixes-20261107-549a71b0f1ae46c08836a3639f47cd44.
  275controlfiles unchanged,backup/rollback dry-runOK;319tests иCIWindows/Linux
  3.10/3.12 passed. ВидимыеRGBA/alpha/size/slots прежние. Игровая приёмкаpending.
  Полная база/протокол — converter docs/equipped-alpha-navigation.md.

## Смотрители и все кольчуги — 6 ноября, актуальное продолжение

- Converter fix/gatekeeper-equipment, draft PR45 поверх PR44, code8c23b4a.
  KEEPER14916/GATEWA14917: native halberd4412, armor6039/6019, без щита.
  One-time BGE1; source0 старого save распознаётся только по полной сигнатуре
  десяти Knight items. Чужие вещи сохраняются; нет spawn/refill после кражи.
- Установлены3modfiles в F:, receiptv10 final journal
  interaction-fixes-20261106-40ba071011bf4848bbcf84a1d92b4732.
  Промежуточный3a4de7daffc64a99b6eae8e959b2ebaf откатывать после final.
  238controlfiles unchanged; backups/rollback dry-run OK. Saved equipment
  файловым откатом не отменяется. D:/saves/engine/INI/launcher не менялись.
- Engine fix/bg-chainmail-variants, draft PR24 поверх PR23, code0111159.
  Fine18400:12native chain torso banks + адресные6296/6301/6341 shared-banded
  исключения. Материал/addmesh вместе, PC/NPC/preview на всех картах;
  характеристики и saved fields не меняются.16resource dependencies найдены.
- Portable0.2.117, CI42265171561 success, Build128055098196:
  registry/policy,6hooks,13images OK. Artifact13527775108,23,573,969B,
  GitHub ZIP SHA256 a137466fa2ac8cf403ae2039385486fe10835e306f0f681cdee451c530228f7a.
  https://github.com/DnDuncan/TemplePlus/actions/runs/42265171561/artifacts/13527775108
  Распаковка движка вручную пользователем в НОВУЮ папку117. Не ставить EXE
  автоматически и не перенаправлять launcher. Игра/CUA не запускались.
- Full local297unittest +15runtime py_compile OK. Converter CI42265971943 /
  42265963188 success Windows/Linux3.10/3.12. Игровая приёмка впереди.
  Протокол и ограничения: converter docs/gatekeepers-chainmail.md.
- Полный Кэндлкип не завершён. IWD2icons кроме MageArmor pilot ещё pending;
  merchant5concepts ждут выбора, цены52refs пока таблица без изменения игры;
  другие blocked areas AR2617 ещё не объявлять исправленными. HALB01 store
  legacyalias4118 не мигрировался этим этапом. Все PR draft, не сливать.


## Расходники — 6 ноября, продолжение

- PR23 feat/bg-consumable-identification, code f1dc48c, portable0.2.111.
  Full CI42245619750 success, Buildjob127986946012, registry/policy +6hooks
  +13image tests OK. Artifact13517867527,23,573,764B, SHA256 GitHub ZIP
  9ac925603051ceee2f8ae71930dc4b058b9de0f29da61e56d6e6683c3c8d1cb2.
- inventory.cpp: automatic OIF_IDENTIFIED on scroll/food insertion and query
  in BG scenes; originalDoorTerrain null-guard required during init. Other
  maps/items and class/UMD requirements unchanged. Native unidentified-scroll
  refusal in PerformCastItemSpell is a confirmed use blocker; Spell280 is full.
- Converter PR43 installed4modfiles: IE/BCS migration for existing known
  inventories and one IWD2 Mage Armor icon (9280→invAid24000, SPWI102A).
  No spawning/restock. Rollback files does not revert later saved item flags/
  icon fields; keep resource registration for such saves or reverse migration.
- Converter current feat/native-price-catalog PR44:52mapped native prices,
  column52 worth, column53 icon. Text splitlines breaks VT-separated flags;
  byte LF reader required. Full292tests +Windows/Linux CI pass; gameplay
  untested. No ToEE/CUA. Engine installation remains manual by user.



## Актуально — 6 ноября 2026: ходьба

- Пользователь сам проверяет в игре позже. Computer-use и запуск ToEE запрещены.
  Не трогать основную D:, saves/INI/launcher. Движок распаковывает пользователь.
- `feat/bg-party-walk-toggle`, draft PR#20 поверх #19. Код55ca05ddf8c90f5a23b260216d3a01c12031c192,
  **0.2.104**, full CI42089416908 success, Build job127463217738. Portable
  artifact13445696434,23,573,721B, GitHub digest
  f77020f0a16e33cc7b3e8826696a06b7b1f4e5b2d8fca4ccd9f3f81f0939d524.
- Native vector button со следами внизу справа, tooltip, Shift+R. Простая R
  сохраняет отдых; Ctrl временно инвертирует режим. Persistent config
  partyMovementMode=-1(native)/0(walk)/1(run). Область применения — BGparty
  вне боя, не NPC. Бой/диалог/инвентарь/console/picker/manual roll блокируют toggle.
- Formation fix использовал PushRunToTile напрямую: он тоже подключён к
  новой политике. PushRunNearTile создаёт ag_move_near_tile для ходьбы;
  native0x1001C1B0 stores radiusFeet в **animId.number**, не range.number.
- Reissue обычного движения находится в **AnimSystem::RefreshPartyMovement**:
  GetSlot private, не вызывать его из UI. Не прерывать action/spell/dialog roots.
  Windows macro `near` не использовать для переменной. RenderText требует
  mutable TigRect lvalue. CRLF/BOM старых исходников сохранены.
- Native standalone registry/movement rules,6save hooks и engine image tests
  в CI прошли. Сам UI и движение в игре не проверялись — это делает пользователь.
- Converter PR#35–39: one-time WATCH source equipment и общий store/dialog
  item lookup установлены отдельно в F: с journal/rollback; не требовали новогоEXE.
  Полный source graph9areas/73actors/37containers. Шесть remaining interiors
  и50NPC/38dialogs подготовлены **offline**. Не расширять native scene IDs и
  не подключать пакет без интеграции маршрутов/условий,actors/loot/townmap.
  Source alarms,23unsupported actors,28itemrefs,combat/cutscene ещё pending.
- Полная база: AGENTS.md и docs/build-0.2.104-windows.md текущей converter
  ветки fix/candlekeep-quest-item-index. Все PR draft, auto-merge не выполнять.



## Новый этап — 5 ноября 2026

- Пользователь принял 0.2.96 целиком. Ветки: feat/bg-inn-upper-floor поверх
  fix/bg-container-actions-roll-ui (#17), конвертер feat/inn-upper-floor поверх #32.
- Без computer-use и запуска игры. Пользователь проводит игровые проверки позже;
  работать без подтверждений. Движок скачивает пользователь, D: не трогать.
- API v6: третья изолированная сцена (логическая 5191, native 5136), лестничные
  passage markers без фиктивной створки. Каждый маршрут проверяет свой gate.
  Разделённая группа и all-or-nothing размещение используют принятый механизм.
- Актуальный протокол и результаты сборки записываются в конвертере:
  docs/candlekeep-upper-floor.md и AGENTS.md ветки feat/inn-upper-floor.


## Текущий этап — 4 ноября 2026

- Ветка `feat/bg-locked-object-checks`, PR #16 поверх
  `fix/bg-candlekeep-doors-search-occlusion` (#15). База 0.2.78 принята
  пользователем по дверям и маскам. Конвертер: `feat/locked-object-checks`, PR #31.
- Пользователь **запретил computer-use и запуск игры**; он сам проверяет сборку.
  Не трогать D: и сохранения. Движок скачивает/распаковывает пользователь в
  новую отдельную папку, сохраняет 0.2.78. Старый launcher не перенаправлялся.
- API v4: контекст дверей на source-контейнерах, native GUID/инвентарь/замок/
  ловушка; read-only preview Guidance; ручные Roll/Result/Continue и одноразовый
  commit; собственный нумерованный d20; серые скрытые части модели; отображение
  кольчуг банком Co8 Fine Chainmail 18400 для материалов и addmesh.
- `ui/ui_bgrolls.*`, `ui/bgmanualcheck.inc`, `ui/ui_bgcheckresources.*`,
  `bgcheckstate.h`, `ui/bgdicegeometry.h`, `bgappearance.h` — новые части этапа.
  Бонусы идут через native BonusList; расход — native Debit/ItemSpellChargeConsume.
  Каталог: Guidance, Heroism/Greater Heroism и пять усилений характеристик,
  подготовленные/спонтанные заклинания и зелья. Дальние источники группы видны
  серыми `[far]`, но не используются через стены/между сценами. Палочки,
  свитки, bardic music и прочие активируемые эффекты ещё вне каталога.
- Остальные синхронные native-проверки пока имеют прежнее окно. Атаки и
  спасброски не переделывались. Никакие ресурсы игр/DSN/BG3 не копировались в Git.
- Проверен Windows build 143b8e0 / CI 41855118690 / 0.2.84, артефакт 13338281268.
  После него 0ab5aa2 добавляет отображение дальних источников; его CI выполняется.
  Все 20 landing-позиций d20 и lifecycle проверки проходят standalone тесты;
  шесть Python save-hook тестов проходят. Игровая приёмка нового этапа впереди.
- Установлен только API guard конвертера (journal
  `F:\Documents\ToEE_BGfication\.bg2toee-deploy\interaction-fixes-20261104-f574bddeffcc4597a617bbbcdd7e253e`).
  Откат dry-run прошёл, 56 контрольных файлов не менялись.
- Подробная текущая передача и ручной протокол — `AGENTS.md` и
  `docs/candlekeep-locked-objects.md` в ветке `feat/locked-object-checks`
  репозитория bg2toee. Указания и пути ниже относятся к старому этапу.

Этот форк — движок для мода **bg2toee** (Baldur's Gate EE на движке ToEE).
Полная база передачи работы: `AGENTS.md` в репозитории `DnDuncan/bg2toee`
(ветка `fix/candlekeep-playtest`). Отвечать пользователю по-русски.

- Рабочая ветка: `fix/bg-candlekeep-playtest` (PR #13 → `feat/candlekeep-objects`).
- Сборка только в GitHub Actions (`CI`, Windows/MSVC, ~11 мин), артефакт `TemplePlus`;
  пользователь сам скачивает его и распаковывает в
  `F:\Documents\ToEE_BGfication\bg2toee-engine\6f35ee9e8e44\`.
- BG-код: `TemplePlus/graphics/mapterrain.*`, `graphics/originaldoor*`,
  `graphics/sourcecontainers.h`, `python/python_game.cpp` (`game.bg_*`),
  `ui/ui_bgjournal.*`, `ui/ui_bgrolls.*`, `skill.cpp`, `animgoals/animgoals_callbacks.cpp`,
  `gamesystems/legacymapsystems.cpp`, `Infrastructure/mdfparser.cpp` (`ZGreater`),
  `tpdata/templeplus/lib/templeplus/savehook.py`.
- Новые `.cpp/.h` регистрировать в `TemplePlus/TemplePlus.vcxproj` (UTF-8 BOM, CRLF);
  у файлов с CRLF сохранять CRLF.
