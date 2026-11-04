# AGENTS.md — форк Temple+ для bg2toee

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
