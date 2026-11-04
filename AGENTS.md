# AGENTS.md — форк Temple+ для bg2toee

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
