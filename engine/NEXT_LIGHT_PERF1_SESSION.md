# Продолжение после light-perf1 — 2026-11-09

Прочитай `SKADOVSK_LIGHT_PERF1_REPORT.md` и `evidence-summary.json`. Актуальное дерево **C:/Users/User/Documents/ChatGPT/stalker/OpenXRay-Android**, без `.git`; старая копия work/openxray-build отстаёт. Не заменять новейший код старым архивом.

Установлена **1.6.02-light-perf1**, Release arm64-v8a, SHA256 `10cea6d522e50dcded367b5ed804acb0ed4899a31333a4b396b5366e34330f46`, package org.openxray.dev, UID a369. Она содержит предыдущий light-scene-diag schema3 и новую консервативную broad-phase для деталей локальных теней. Нет новых изменений физики/AI/лаунчера.

Только три файла оптимизации, `changes.json` задаёт SHA; combined `light-perf1.patch` накладывается на light-scene-diag. Не накладывать поверх perf1 повторно. Обычный draw loop сохранён. Включено default, флаг `-gles_detail_eager_setup` временно возвращает прежний eager/per-instance алгоритм. Финальные Native A/B: 29.16 / 46.16 FPS, ANGLE: 25.52 / 38.91 и 39.91 FPS, только один фиксированный ракурс, GPU финальных runs не измерен.

Пользовательские 23 защищённых файла побайтно восстановлены/проверены. Аргументы -gles_gpu_profile, Native GLES, Diagnostic profile вернулись; CoP density .56 и radius89, 1728x1080. Игра завершена, время перед quit возвращалось 10, экран Dozing. Не использовать старые октябрьские user.ltx backups с другими параметрами.

Следующий конкретный шаг: пользовательская проверка в движении у Скадовска и ближних локальных lights, плюс обычный smoke SoC. Не объявлять весь Затон или нагрев исправленными. При новом профилировании сначала запустить uninterrupted `-gles_gpu_profile` без HOME/lock между загрузкой и записью: после наших pause/resume queries не завершались, GPU rows=0. Причина неизвестна. Не делать широкую переработку lifecycle по гипотезе и не смешивать её с этим патчем.

Raw исходные исследования/прототипы: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/skadovsk-light-lazy1-20261109, C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/skadovsk-light-bounds2-20261109. Финальные source/build/tests/restoration/package evidence: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/skadovsk-light-perf1-20261109. Сохранены lazy1/bounds2 APK и baseline light-scene-diag. Ничего не опубликовано, Git push не выполнен.

Если понадобятся ADB helpers, брать именно L/cop_launch.py, L/cop_console.py, L/prepare_case.py, L/walk_probe.py — они адаптированы для UID369; старые helpers смотрят UID364. Консоль закрывать ESC111 с визуальной проверкой; после load она может закрыться сама, после quit ESC не отправлять в launcher. Между tests HOME+SLEEP223. Не писать original quicksave; benchmark codex_skad_bench_20261031.scop.
