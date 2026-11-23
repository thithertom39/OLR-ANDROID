# R2 — сохранённый частичный результат, 23.11.2026

Работа остановлена после проверенных исправлений и сохранения APK. Самостоятельно
не возобновлять R1 и не начинать новые направления. Стабильные 60 FPS и полный
визуальный/GLES регресс не доказаны. Следующее действие — отдельный запрос пользователя.

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
