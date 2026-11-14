# Stability candidates: исправления и проверки

**Followup D2,23.10.2026:** новые независимые варианты и их доказательства описаны в STABILITY_FOLLOWUP_RESULTS.md; handoff обновлён в NEXT_STABILITY_SESSION.md. TaskQueue4 variants/4 NDK TU/300000 tasks; DEC v2/4 patches/4 NDK TU/6 detected mutations; EGL19 policy tests (не renderer); Launcher200/100/400 extracted cycles+optional journal; Mods49 host tests. Всё новое находится вне production R и не собрано в общую APK. Таблицы ниже относятся к прежнему D1 набору, а не к применению D2 кандидатов.

23 октября 2026. **Все результаты ниже offline.** Ни ADB, ни установка, ни аппаратные циклы не выполнялись. Слово PASS относится к указанному fixture/translation unit; оно не означает полную стабильность игры.

## Независимые изменения

| Группа | Production scope | Доказательства / повторение |
|---|---|---|
| L01 | `SDLActivity.java`, освобождение Clipboard listener перед nativeQuit, запрет поздних callbacks | D/launcher/candidate1-results.json, final-evidence.json; 1000 mock cycles, baseline накапливает observers, candidate 0 |
| L02 | `AppLog.java`, одна process exception wrapper | 64 installations: одна запись/flush/default delegation; mutation guards |
| L03 | `CrashReporter.java`, явные Java/native signatures | 10/10 cases; baseline 4/10; обычные signal/abort слова не считаются доказательством |
| L04 | `AppLog.java`, по одной app-only reopen точке Launcher/Game | Append без rotation/новых logcat threads; оба порядка resume/destroy, повторные reopen и прежние backup bytes PASS |
| L05 | `LifecycleTrace.java` и редкие callbacks обеих Activity | 100 mock cycles, 200 instances, 2000 transitions +400 concurrent records; 12 super calls сохранены. Это инструментирование, не исправление вылета |
| ENG-001 | `x_ray.cpp`, bounded `-ltx/-fsltx` | D/engine/after-regressions.json, boundary/canary и mutation fixture |
| ENG-002 | `saved_game_wrapper.cpp`, минимальная длина header+payload | Исходный короткий header ломает fixture; кандидат отклоняет, valid control PASS |
| ENG-003 | Там же, r_open guards + RAII preview buffer | Valid preview и early-return paths, ASan/UBSan/leaks PASS |
| ENG-004 | `alife_storage_manager.cpp`, close invalid stream | Оба исхода проверки с нулём открытых readers |
| ENG-006/007 | `script_engine.cpp`, free old scratch buffer / unregister main VM | D/engine/lua-fixed-results.json, lua-address-results.json, root-lua-verification.json и lua-ndk-results.json; разные/reused VM, allocation failure/retry, preserved valid order |
| GL001..004 | `glSH_RT.cpp`, `glSH_Texture.cpp` | D/gl/host-results.json: 13 final cases, 4096 RT и4096 texture reuse cycles. 2000 valid sequences/14000 frame choices; invalid headers и256 video-error cycles; ASan/UBSan/NDK |
| FS01..05 | `LocatorAPI.h/.cpp`, `IGame_Persistent.cpp`, `UIGameCustom.cpp` | D/filesystem/summary.json; real POSIX actual-method fixtures, 10/10 current +8/8 overlay cases; 3 actual NDK TU |
| CON-001 | `device.cpp`, `frame_trace.h`, `x_ray.cpp` destructor hook | D/concurrency/watchdog-queue-results.json; 20 install/stall/shutdown/restart cycles, duplicate/start failure, pending signal drain, previous disposition/mask, ASan/UBSan |
| CON-002 | `Threading/Event.cpp` | D/concurrency/event-results.json; 14 deadline/boundary/wake/auto-reset/error cases, ASan/UBSan, NDK |

Патчи и preimages сохранены по группам в D/launcher/patches, D/engine/ENG-*.review.patch, D/root-fixes, D/gl, D/filesystem, D/concurrency. Некоторые файлы участвуют в нескольких последовательных независимых патчах: откатывать обратным порядком после проверки preimage/current SHA, не восстанавливать один старый файл поверх более поздней правки вслепую.

## Проверки компиляции и их пределы

Использованы JDK17, Android NDK27.0.12077973, API24 ARM64 Release, WSL2 Ubuntu24.04 g++13 для ASan/UBSan/leak fixtures. Обычная сборка сохраняет Native GLES и ANGLE pair, 16K alignment, прежний shared C++ runtime, подпись и renderer dispatch. GPU profiler и новые scheduler/mod prototypes не включаются.

Actual NDK TUs: RT/Texture profiler OFF/ON; x_ray/saved_game_wrapper/alife_storage_manager; script_engine; device/Event; LocatorAPI/IGame_Persistent/UIGameCustom.

Полная staging-сборка **BUILD SUCCESSFUL за 9m13s**, 47 tasks. Java compilation, native link, resources, dex, lintVitalRelease, packaging прошли. Полный lintRelease не запускался; warnings не устранены и не названы чистым анализом. Финальный verifier PASS: прежний сертификат/v2, 29 ARM64 ELF/16K, Native+ANGLE, shared runtime/dispatch, frozen sources/dependency hashes, LifecycleTrace в DEX и пустой GitHub token.

Артефакт W/outputs/openxray-soc-stabilityaudit1-arm64.apk: **33 698 572 байта**, SHA256 `adc06bb3dcf9dba378532f4b8ae07a182b360a3973514adb7999850238689fbf`. Не установлен, аппаратно pending. Отчёт D/pipeline/stabilityaudit1-apk-report.json; полные журналы D/pipeline/build-*.log. Две thermal A-правки в этот кандидат не входят, старые APK сохранены.

Извлечение production bodies уменьшает риск тестировать переписанный пересказ. Mock GL/Android/FS доказывает выбранные lifetime/branch contracts. Mutation tests показывают, что проверки замечают соответствующие регрессии. Ни то ни другое не исполняет настоящий Adreno driver или Android framework.

## Явно исключённые и незавершённые проверки

- Исторические `fixed-command-scan-oversized` и `fixed-lzo-malformed` в общем engine runner намеренно запускают старое выражение/неизменённый decoder; это **не** провал исправленного bounded parser и не PASS decoder fix. Читать label/source и отдельные acceptance checks.
- Ранний Lua запуск, начатый до применения root-правок, сохранён в D/engine/lua-pre-root-invalid и не используется. Приняты только последующие source-stable результаты.
- TSan в этой WSL среде не работает; race-free заявлений нет.
- Unmanaged coroutine registry, настоящий context loss, очередь scheduler, malformed `.db*` parser/decoder и atomic engine cfg/assets остаются отдельными задачами.
- Файловый `overwrite=false` защищает известный физический destination, включая dangling symlink; cross-process TOCTOU между lstat и rename не устранён.
- Не измерялись FPS, расход энергии, задержка управления и качество изображения после новых патчей.

## Изоляция APK и откат

Рабочее дерево сохраняет исходные thermal A-изменения. Для stability candidate используется отдельный staging tree: в нём две thermal A-функции возвращаются к сохранённой pre-thermal базе, а stability patches берутся из нового source freeze. Это не reset рабочего репозитория. Dependency versions/источники не обновляются, для staging выделяются отдельные dependency build dirs.

`cordonperf1` SHA256: `22f0ec05d00ee25ad9f62a4aafcbddc99d92eee98ca92e8f4a93d0d3e5738df2`.

`thermala1` SHA256: `e54eccbedfa879cc3c9efedf4c492c31f50de84b8a759fd33c1f9f195c02b4dc`.

Не заменять ими друг друга. Не устанавливать stability APK до успешного thermal restore. При аппаратной регрессии сохранить логи и вернуть сохранённую рабочую APK обновлением с прежней подписью; не выполнять uninstall/clear data ради удобства.
