# R2 — работа возобновлена, 23.11.2026

Пользователь возобновил оптимизацию: прежние P95 21,62 мс не достигают цели.
Продолжать R2 до подтверждённого P95 <=16,67 мс при 1920×1080 в тяжёлой сцене
либо реального ресурсного/технического блокера. R1 не возобновлять.
При остатке нескольких процентов лимита безопасно сохранить всё для продолжения
пользователем или другим чат-ботом; не оставлять на планшете регрессивный эксперимент.

Новый код a51efb9: счётчики типов геометрии, отложенная привязка VB и пропуск удаления
пустого shader handle. Сборка и запуск на Adreno прошли; проблема 60 FPS ещё не решена.
Незакоммиченный эксперимент: glStaticIndexBatch.h и связанный immutable index cache.
Все текущие результаты: logs/resume-20261123. Не считать его проверенным ускорением,
пока не завершён сопоставимый A/B. Потоковая версия с BufferSubData дала сильную
регрессию и отвергнута (копия только в logs/.../glStaticIndexBatch-streaming-regression.h).
EXT_multi_draw_arrays на планшете отсутствует, прототип исключён; patch сохранён
локально в artifacts/unsupported-ext-multidraw.patch. Ничего не удалять.

Новые пользовательские логи АТП: logs/resume-20261123/user-atp. При приближении к
АТП: 61 shadow pass, 7420 static caster entries, local wall ~10,5 мс. Это подвижный
захват при 1728×1080, не переносить его числа на Full HD или неподвижный A/B.
Воспроизводимый ATP probe: save r1_checkpoint_20261121, actor feet (44.93,2,22.07),
yaw +1.927; отрицательный yaw смотрит в лёгкую противоположную сторону.
Параметры: -start server(r1_checkpoint_20261121/single/alife/load) client(localhost)
-$run_string db.actor:set_actor_position(vector():set(44.93,2.0,22.07));db.actor:set_actor_direction(1.927)
Важно: между -$ и run_string пробела нет. Перед тестом vid_mode в _appdata_/user.ltx
должен быть 1920x1080; одного выбора в лаунчере недостаточно. Проверять width в JSONL.
Использовать Normal (play), а не Diagnostic (logs), который добавляет тяжёлые аудиты.

Исходная актуальная конфигурация пользователя и saves сохранены в
logs/resume-20261123/device-before. До завершения вернуть её побайтно и проверить
SHA сохранений. Прежний отчёт ниже — историческая контрольная точка, не итог новой работы.

Полный отчёт: [docs/R2_FINAL_20261123_RU.md](docs/R2_FINAL_20261123_RU.md).

R2 worktree R:\project\OLR-R2-final, ветка perf/olr-r2-cordon-20261123,
draft PR #9. Стабильная база 4b07c0e; experimental R1 commits не импортировались.
Последний код: 2563e5332c30190849eab00832d993649af8004b.
CI: https://github.com/thithertom39/OLR-ANDROID/actions/runs/45133494174 — success.

Установленная APK: artifacts/OLR-R2-final-2563e53-arm64.apk.
SHA-256: 049D56C779357BD2254499BFF4BCB2686C30744F7CF459637AA79C62A24E130C.
Manifest: artifacts/final-verified.json; обычная APK без GPU-profile hooks.
Все предыдущие APK и captures сохранены. Ресурсы игры/keystore/логи/APK не коммитить.

Full HD +X: frame P95 24.99 → 21.62 мс, P99 29.00 → 22.95 мс;
local-light render-thread CPU P50 5.89 → 2.95 мс. Main geometry CPU 7–8 мс,
около 3300 draws остаются. Это короткие sampled A/B со слегка различным yaw,
не гарантия FPS во всех сценах. GPU timestamps — timeline, не чистый busy time.

Исправлены повторные detail-instance bounds tests и обработка несовместимого
autoload save. olr.sav version 3 сохранён без конвертации; ожидается version >=7,
теперь отказ возвращает в меню. Контрольный save загружается. 17,879,303 reference
tests: 0 mismatches. GLES vertex-format probe: 85/85; conservative bounds: 122000.

Остаются одинаковые baseline/final KHR_debug сообщения удаления shader objects
(64 до лимита), полное GPU execution profiling и регресс других игровых условий.
См. logs/gles-debug-comparison.json, logs/comparison-final-1728.json,
logs/comparison-1920.json, logs/final-save-guard-invalid, logs/final-valid-save.

User.ltx восстановлен побайтно; 4 сохранения проверены SHA-256, не изменялись.
Launcher: исходный 1080p Screen 1728×1080, custom args пусты. На планшете игра
закрыта, экран выключен; параметры thermal/performance не менялись.

R1 заморожен на f36351c55cfc7ed3080931137bd0814dc6583617, draft PR #8.
Точное продолжение R1: R:\project\OLR-R1-parity\R1_FROZEN_FOR_R2_20261123.md.
Там остаётся незавершённый независимый R1, включая неподдержанные материалы и
недоказанную actor shadow-only route; не объявлять его завершённым.
