# OpenXRay Android — системный аудит стабильности

**Актуализация D2 от23.10.2026:** продолжение в STABILITY_FOLLOWUP_RESULTS.md и актуальном верхнем блоке NEXT_STABILITY_SESSION.md. Новые TaskQueue/DEC кандидаты, EGL dependency plan, дополнительные launcher tests/journal и mod foundation находятся только в work/stability-followup-20261023. Код R и protected APK не изменялись; аппаратной приёмки нет. Ниже сохранён первоначальный D1 аудит, его открытые production-проблемы не закрываются наличием внешнего кандидата.

Дата: 23 октября 2026. Работа выполнена без планшета и контроллера, без ADB. Это аудит конкретного рабочего дерева и локально проверенные кандидаты, **не аппаратная приёмка**.

## Главный результат и границы доказательств

Пользовательский сценарий `Launcher → Game → Exit → Launcher → Settings → crash` на устройстве не воспроизведён. Java exception, native crash, ANR, process death и повторный запуск Activity пока нельзя выбрать как установленную причину. Наличие session marker само по себе не доказывает crash.

Наиболее существенная подтверждённая находка для этого сценария: Android watchdog движка отсоединялся навсегда и сохранял pthread игрового потока после завершения SDL main. Source-extracted тест подтвердил попытку отправки сигнала после моделируемого завершения сессии. Подготовлено явное stop/join до уничтожения движка. Причинная связь с вылетом пользователя — **Hypothesis**, несмотря на Confirmed lifetime defect.

Дополнительно воспроизведены дефекты GL reset/reload, файлового rename и архивов, обработки сейвов, жизненного цикла Lua, clipboard callbacks и диагностики. Патчи независимы, с preimage и проверками. Большой scheduler не переписан; менеджер модов не включён в production.

## Сохранность и организация

- R: `C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build`.
- D: `C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/stability-20261023`.
- До изменений сохранено реальное рабочее дерево: D/source-baseline.zip, **11 724 файла**, 53 390 073 байта, SHA256 `28977feb0126f6654e056a2083274a916cdd63158cb6ed29de832cdef744b86b`. Манифест D/baseline-manifest.json. Предыдущие незакоммиченные изменения включены; Git reset/clean/checkout не выполнялись.
- `cordonperf1` и `thermala1` сохраняются отдельными неизменными APK. Сейвы/настройки устройства не читались и не менялись в этой задаче.
- Работа распределена по Android lifecycle, native correctness, GL lifetime, concurrency, filesystem/configuration. Root проверял пересечения владения, точечные diff, условия сборки и доказательства.
- Нет новых performance/thermal изменений, изменений графического качества, AI, физики или баланса. Предыдущие две thermal A-правки остаются в рабочем дереве как исходная база, но исключаются из отдельной staging stability APK. Mod foundation существует только вне APK.

## Реестр существенных находок

Confirmed ниже означает воспроизведённый локальный дефект исходников/контракта, а не доказанный вылет пользовательского сеанса.

| ID | Severity | Confidence | Участок / последствие | Состояние |
|---|---|---|---|---|
| CON-001 | High | Confirmed; crash attribution Hypothesis | `device.cpp::ft`, watchdog переживает engine session, хранит завершившийся pthread | Source candidate stop/join + shutdown hook; host/NDK PASS |
| L01 | Medium | Confirmed | SDL ClipboardManager сохраняет listeners и поздние JNI callbacks | Source candidate release + late callback guard; mock PASS |
| L02 | Medium | Confirmed | AppLog повторно оборачивает default exception handler | Source candidate идемпотентной установки; mock PASS |
| L03 | Medium | Confirmed | CrashReporter путает явные Java/native signatures | Source classifier candidate; 10/10 fixture PASS |
| L04 | Medium | Confirmed | После Game destroy закрыт app log, Launcher может продолжать без записи exception | Append-only reopen, оба порядка Activity callbacks PASS |
| GL-001/002 | High | Confirmed | CRT/CTexture сохраняют удалённые GL names: reset/reload не создаёт ресурс; повторный delete затрагивает переиспользованное имя | Source candidates; actual-method mock GL + sanitizers + NDK PASS |
| GL-003 | High | Confirmed | `.seq` FPS 0 или >1000 приводит к делению на ноль | Header validation candidate; valid sequences preserved |
| GL-004 | Medium | Confirmed | OGM GL-error теряет созданную texture | Source error-cleanup candidate; injected-error fixture PASS |
| ENG-001 | High | Confirmed | `-ltx/-fsltx` неограниченно читаются в stack buffers | Bounded token candidate; границы/canaries/NDK PASS |
| ENG-002 | High | Confirmed | Проверка сейва допускает 8-byte header, caller читает 12 | Minimum header/payload guard candidate; source fixture PASS |
| ENG-003/004 | Medium | Confirmed | Failed r_open / preview early returns / invalid load reader | Null guards + RAII + reader close; fixtures PASS |
| ENG-006/007 | Medium / High | Confirmed | Lua reinit теряет scratch buffer; destructor оставляет main VM registry entry | Independent candidates; ASan/UBSan + NDK PASS |
| FS01 | High | Confirmed | POSIX archive fd 0/-1 трактуются как bool; повторное открытие ломается | Explicit is_open/sentinel candidate; вызывающие места обновлены |
| FS02 | High | Confirmed | unload_archive удаляет только одну индексную запись | Удаление всех записей этого архива; fixture PASS |
| FS03/05 | High | Confirmed | self-rename обращается к освобождённой записи; failed rename теряет destination/index; unindexed destination перезаписывается | Minimal preservation candidates; real POSIX fixtures PASS; cross-process TOCTOU остаётся |
| FS04 | High | Confirmed | Bare/oversized `-overlaypath` нарушает безопасный разбор | Bounded parser candidate; 8/8 PASS |
| CON-002 | Medium | Confirmed | POSIX timed Event получает ненормализованный timespec, игнорирует EINVAL | Normalization/error handling candidate; 14 cases PASS |
| CON-003 | High | Confirmed | TaskQueue публикует свободный slot до окончания работы consumer: потеря нового task | Production не изменён; exact-capacity reproducer и отдельный sequence prototype |
| ENG-005 | High | Confirmed | `rtc_decompress` использует небезопасный decoder; Release VERIFY не проверяет failure | Открыто: общий контракт ошибки для архивов/сейвов требует отдельного решения |
| GL-005 | High | Confirmed limitation | Настоящая потеря EGL context не вызывает полного восстановления всех engine resources | Не реализовано; не приравнивается к обычному HOME/resume |

Полные сценарии, функции, риски и тесты: [LAUNCHER_CRASH_INVESTIGATION.md](LAUNCHER_CRASH_INVESTIGATION.md), [ENGINE_CORRECTNESS_AUDIT.md](ENGINE_CORRECTNESS_AUDIT.md), [GL_RESOURCE_LIFETIME_AUDIT.md](GL_RESOURCE_LIFETIME_AUDIT.md), D/concurrency/audit.md, D/filesystem/audit.md. Не все предупреждения статического анализа являются ошибками: другие scheduler races, borrowed texture ownership, необслуживаемые Lua coroutine keys и неполные crash markers перечислены отдельно по confidence.

## Что ещё не принято

Отдельный кандидат **1.6.02-stabilityaudit1** собран и проверен: W/outputs/openxray-soc-stabilityaudit1-arm64.apk, SHA256 `adc06bb3dcf9dba378532f4b8ae07a182b360a3973514adb7999850238689fbf`. Полная сборка 9m13s и artifact verifier PASS. Native/ANGLE payload, подпись/v2, 29 ARM64 ELF/16K, исходники и изоляция thermal проверены. **Установка и runtime не выполнялись.** Подробности D/pipeline/stabilityaudit1-apk-report.json и STABILITY_FIXES_AND_TESTS.md.

1. Не завершено восстановление после thermal-session. При следующем подключении первым делом выполнить предусмотренный `work/thermal-power-20261023/restore_today.py --run`; проверить exact user.ltx, 28 persisted UI states, 12 save hashes и stopped/screen OFF. До этого новую APK не устанавливать.
2. Ни один новый патч не прошёл устройство. Нельзя объявлять пользовательский вылет устранённым, повторный запуск полностью надёжным, картинку идентичной или производительность сохранённой по одной компиляции.
3. Нужны нормальные циклы выхода игры, не повторный MAIN поверх живой GameActivity. План: D/launcher/plan_launcher_cycles.py; сначала минимум 10 циклов каждого backend с логами, затем HOME/Recents/lock/resume и существующий сейв.
4. Queue replacement, полное context recovery и общий decoder error path требуют отдельных кандидатов. Не добавлять их незаметно в ту же APK.
5. Full sanitizer coverage engine и рабочий TSan отсутствуют. WSL TSan завершался `unexpected memory mapping`; ASan/UBSan проверяют выбранные extracted paths, не весь Android runtime.

## Основа менеджера модов

После основного аудита подготовлены архитектура и изолированная reference-модель: `Mod`, manifest parser, pinned profiles/dependencies, conflicts, metadata resolver, bounded ZIP inspection. **28 тестов на Windows и WSL**, в том числе 2000 путей. Production launcher/CLocatorAPI/Gradle не подключены к этим компонентам.

Исследован существующий `CLocatorAPI`: один победитель на имя, last registration wins, нет fallback chain; alias roots, `.db*`, `vfs_idx`, resource caches и Android case sensitivity накладывают ограничения. Расширять нужно существующий locator, а не создавать параллельную рабочую VFS. ZIP inspection не равен установщику; `.db*` в первоначальном прототипе не принимаются. Подробности: [MOD_MANAGER_ARCHITECTURE.md](MOD_MANAGER_ARCHITECTURE.md).

Продолжение и аппаратные gates: [NEXT_STABILITY_SESSION.md](NEXT_STABILITY_SESSION.md). Доказательства и ограничения исправлений: [STABILITY_FIXES_AND_TESTS.md](STABILITY_FIXES_AND_TESTS.md).
