# Исследование возврата из игры в Android-лаунчер

Дата: 23 октября 2026. Исследование выполняется **без планшета, без ADB, без изменения пользовательских данных**. Текущее сообщение пользователя — регулярное исчезновение лаунчера после обычного выхода из игры и последующего изменения настройки. Этот сценарий **не воспроизведён в текущей сессии**; matching AndroidRuntime/tombstone/ANR evidence пока нет. Наличие исправленных отдельных дефектов ниже не доказывает устранение этого вылета.

## Обязательная последовательность при следующем подключении

До новой установки восстановить thermal-session: `work/thermal-power-20261023/restore_today.py --plan`, затем root-оператором `--run`; проверить точный SHA256 исходного user.ltx `b129ad651af57f7191734fe91dc98a80d9a1e85931e978a826bccbf94dd568b8`, 28 UI-состояний и все 12 saved files, package stopped/screen OFF. Подробности: `NEXT_THERMAL_OPTIMIZATION_SESSION.md`. На планшете сейчас установлен thermala1; аппаратная приёмка и восстановление ещё pending. Не использовать старый restore, меняющий глобальный `svc power stayon`.

## Фактическая цепочка Activity и native lifetime

1. Manifest `android/app/src/main/AndroidManifest.xml`: LauncherActivity — exported MAIN/LAUNCHER, OpenXRayActivity — non-exported; отдельный `android:process` ни для одной Activity не указан. Обе Activity живут в одном процессе. У Launcher нет singleTask/singleTop-обещаний. Повторный MAIN поверх игры может создать ещё один Launcher.
2. `LauncherActivity.onCreate` создаёт View tree/skin, открывает SharedPreferences `openxray_launcher_prefs`, запускает AppLog, устанавливает UI listeners, загружает настройки, проверяет config и pending crash marker.
3. `LauncherActivity.launchGame` сохраняет preferences, проверяет `RenderBackendConfig.PROCESS.requiresRestart`, записывает управляемые параметры config и запускает **новую OpenXRayActivity** через `startActivity`. Launcher не finish-ится. Смена backend в том же процессе специально блокируется, поскольку driver libraries и cached pointers process-wide; это не crash.
4. `OpenXRayActivity.onCreate` инициализирует AppLog, ставит session marker, вызывает SDL onCreate. SDL настраивает JNI, вызывает SDL.initialize, назначает static mSingleton/mSurface/mLayout, регистрирует clipboard/audio/HID handlers. GameDataInitThread выполняет подготовку файлов вне UI thread. `isNativeStartReady` защищён mInitLock.
5. `SDLActivity.handleNativeState` запускает SDLThread/SDLMain только после готовности surface, focus, resumed и game data. До готовности ставит static повтор через commandHandler каждые 50 ms. API24+ использует onStart/onStop для pause/resume; onPause/onResume отдельно не сигналят native pause при multiwindow-пути. Это намеренная SDL схема, а не доказанный дублирующий запуск.
6. `SDLMain.run`: nativeRunMain возвращается, Java сбрасывает mSDLThread и вызывает finish на SDLActivity. `SDLActivity.onDestroy` освобождает HID/audio, посылает quit и join при ещё живом SDLThread, затем вызывает nativeQuit. Process exit/kill отсутствует. `OpenXRayActivity.onDestroy` удаляет refresh-rate Surface callback, затем после SDL teardown удаляет session marker и закрывает AppLog.
7. Старый Launcher снова получает onResume. Он перечитывает AA из config и обновляет backend status. Он не создаёт новый view tree, не перерегистрирует все listeners и не загружает весь prefs повторно. SharedPreferences — обычный Android объект; конкретной порчи prefs по текущим исходникам не обнаружено.
8. Следующий запуск с тем же backend разрешён **в том же PID**. Java SDL.initialize сбрасывает часть static state, однако native singletons живут в загруженных библиотеках. Полная корректность повторного SDL_main требует engine/shutdown аудита, описанного отдельно в `ENGINE_CORRECTNESS_AUDIT.md`.

## Реестр находок

| ID | Severity | Confidence | Что установлено | Статус / проверка |
|---|---|---|---|---|
| L-USER-01 | High | Hypothesis о причине; пользовательский симптом подтверждён сообщением | Возврат Game→Launcher→изменение настройки приводит к исчезновению приложения | Нет matching runtime evidence; не объявлять Java/native/ANR тип установленным |
| L-01 | Medium | Confirmed static + actual-source host probe | SDLClipboardHandler регистрирует ClipboardManager observer, но исходный SDLActivity.onDestroy его не удаляет; поздняя доставка идёт в JNI после teardown, каждый запуск добавляет observer | Минимальный source patch и host regression PASS; tablet validation pending |
| L-02 | Medium | Confirmed actual-source host probe | Каждый AppLog.init для нового каталога/после close оборачивает текущий process uncaught handler ещё раз | Idempotent process handler в source, host PASS; это диагностический дефект, а не доказанная причина первого вылета |
| L-03 | Medium | Confirmed static + actual-source host probe | CrashReporter называет logcat `FATAL EXCEPTION` native, а appLog `NATIVE CRASH` — java; обычные `signal`, `abort`, `Build fingerprint` могут дать ложную классификацию | Source classifier использует отдельные явные signatures, host 10/10 PASS; не смешивать с engine behaviour |
| L-04 | Medium | Confirmed source behaviour | OpenXRayActivity закрывает process-wide AppLog при выходе; исходный Launcher.onResume его не открывает. Последующие Java настройки и исключения остаются в system logcat, но file capture уже остановлен | Source append-only **app file** reopen и host PASS; native logcat reader намеренно не перезапускается при resume, для hardware capture нужен root read-only logcat |
| L-05 | Medium | Strong Evidence | Session marker трактуется как process died; он существует и у живой остановленной GameActivity при повторном MAIN/LAUNCHER | Ложный marker dialog уже документирован 22 октября; не принимать за новый native crash. PID/task/activity обязательны |
| L-06 | Low / Medium при teardown | Hypothesis | 50-ms static readiness callbacks не отменяются в SDL onDestroy; нет generation check. Поздний callback может обратиться к static state следующей Activity | Не доказано, что корректный Android event order даёт реальный crash. Изолированный fixture/trace до изменения SDL scheduling |
| L-07 | High при реальном hang | Hypothesis | SDLActivity.onDestroy без timeout join SDLThread на main thread | Нормальный SDL контракт. Для зависшего native shutdown возможен ANR; не заменять join таймаутом и не освобождать native ресурсы, пока поток жив |
| L-08 | Medium | Strong Evidence | GameDataInitThread не имеет cancellation/join: может продолжать запись assets после ухода со стартового экрана. completeGameInit проверяет isFinishing/isDestroyed, но это не отменяет FS работу | Root FS audit; не менять extraction/retry контракт вслепую |
| L-09 | Low | Confirmed static | В showBugTrackerDialog два setNeutralButton: Open tracker заменяется Copy logs | Не crash и не приоритет стабильности; UI polishing отложен |
| L-10 | Low / Medium | Hypothesis | GitHubIssueReporter callback захватывает Activity и после сетевого ответа может пытаться показать fallback dialog уже после destroy | Автоотправка без token отключена в текущих APK; не является доказанной причиной settings-after-exit. Не выполнять отправку без пользователя |

### L-01: границы доказательств

Исходники: `android/app/src/main/java/org/libsdl/app/SDLActivity.java`: SDLClipboardHandler constructor, clipboardSetText, onPrimaryClipChanged, SDLActivity.onDestroy. Фактический SDL C callback из `work/deps/sdl2-src/src/core/android/SDL_android.c::onNativeClipboardChanged` только вызывает SDL_SendClipboardUpdate. Event queue обычно безопасно отклоняет событие после SDL_StopEventLoop. Поэтому нельзя утверждать, что каждый поздний callback вызывает native use-after-free. **Доказаны лишняя регистрация и вызовы после lifecycle boundary**, а не конкретный пользовательский crash.

Probe извлекает реальные class body и onDestroy из рабочего дерева, компилирует JDK17; ClipboardManager/JNI/HID/audio/Activity — моки. 1000 циклов исходной версии оставили 1000 listener, 1 001 000 callback после modeled native shutdown, при этом одно событие перестало соответствовать одному listener. Доставка уже поставленного callback и setText в конце teardown проверяются отдельно. Baseline `work/stability-20261023/launcher/baseline-results.json`.

Кандидат: onDestroy сначала вызывает idempotent release observer. Release, callback и setText синхронизированы; поздний callback игнорируется, teardown setText не регистрирует observer заново. `candidate2-results.json`: 1000/1000 активных single deliveries, 0 listener после destroy, 0 stale native callbacks, 0 повторных подписок. Три mutation проверки (удалить release call, callback guard или re-subscription guard) обнаруживают соответствующие регрессии. Это mock framework/JNI, а не 1000 реальных запусков игры.

### L-02: границы доказательств

Исходник: `AppLog.installUncaughtExceptionHandler`. Исходный метод 64 раза оборачивает predecessor; один Throwable приводит к 64 записям+flush и одной конечной default delegation. На устройстве число повторов зависит от init/close/path смен, а stack overflow при большом числе запусков в текущей сессии не воспроизводился. Исправление должно сохранять default handler Android и реагировать на его смену сторонним кодом, не глотать exception и не подменять системный crash termination.

Кандидат хранит установленный process handler и повторно не оборачивает его. При новой внешней default registration сохраняет её как predecessor; при исходном null всё равно устанавливает logger. 64 повторные установки дают 1 log/flush и 1 default delegation; mutation без guard снова даёт 64 записи. Эта правка не препятствует чужому коду специально создать собственную рекурсивную chain.

### L-03: границы доказательств

`run_crash_classifier_tests.py` извлекает реальные findCrashLine и scanFileFor, создаёт обычные временные host файлы. Baseline 4/10 contracts pass. Не используется реальный AndroidRuntime stream; source probe демонстрирует классификацию входного текста. Нельзя определять process death, ANR или принадлежность чужих строк текущему PID только этим keyword scanner. Старые crash buffers содержат также **другие пакеты** (например com.KonfaGames.Despotism3k); это не доказательства OpenXRay failures.

Кандидат использует явные `NATIVE CRASH`/`Fatal signal` и отдельные `FATAL EXCEPTION`/`FATAL UNCAUGHT EXCEPTION`; broad signal/abort/fingerprint/FATAL word не считаются самостоятельными crash signatures. Все 10 fixture cases PASS. Engine fatal branch сохранён. Marker semantics и автоматическое доказательство ANR/process-death этим патчем не меняются.

### L-04: append-only диагностика после возврата

Новый `AppLog.resumeCapture` вызывается в `LauncherActivity.onResume` и после `AppLog.close` в `OpenXRayActivity.onDestroy`; открывает только прежний app log в append, если writer закрыт. Второй вызов нужен, поскольку Launcher resume может предшествовать окончательному уничтожению старой game Activity. Не ротирует app/backup, не создаёт новый logcat process/thread и не меняет `sInitialized`: следующий штатный game init по-прежнему выполняет свою session rotation/native logcat startup. При отказе доступа пишет ошибку в system logcat вместо падения лаунчера. Fixture извлекает реальные close/write/flush/resume методы: прежние строки и `.bkp` сохранены, 100 повторов resume idempotent, запись настройки после exit появляется в app file, game reinit не подавлен. Baseline без resume теряет эту запись. `run_logging_lifecycle_order.py` дополнительно извлекает фактические AppLog calls двух Activity и проверяет оба порядка resume/destroy: baseline оба FAIL, промежуточный кандидат только с Launcher resume один FAIL, final оба PASS. Это mock порядка, а не наблюдение OEM runtime. Это не сбор native tombstone после выхода — внешний PID-filtered capture всё ещё нужен при аппаратном расследовании.

## Существенная связь с engine audit

Другой агент нашёл отдельный **подтверждённый исходниками lifetime hazard**: detached frame-trace watchdog продолжает жить после SDL_main return и сохраняет pthread_t завершённого игрового потока, затем может послать SIGUSR2. Это намного более прямой кандидат на задержанный native process failure после выхода, чем Java observer leak. Патч/проверки находятся у concurrency аудитора root. Не объявлять его причиной конкретного пользовательского случая без matching exit/crash trace; см. engine/concurrency отчёты.

## Проверки повторного входа: подготовленный сценарий

Пока не подключать/не устанавливать ничего. После обязательного thermal restore:

1. Снять timestamp, APK SHA/version, PID/starttime, task/activity stack, user.ltx hash и save hashes. Запустить непрерывный PID-filtered main/system logcat + отдельный crash buffer. Сохранить stderr при недоступном буфере; отсутствие READ_LOGS не приравнивать отсутствию native crash.
2. Fresh Launcher→Game→главное меню→тот же сейв→штатный Exit→Launcher. Не использовать force-stop, MAIN или второй Launcher для возврата. Пройти и записать lifecycle IDs/времена.
3. По одной изменить обратимо чувствительность, opacity, resolution spinner, AA mode, custom args; отметить конкретный первый trigger. После каждого шага проверить PID и Activity. Важно отличить обычное открытие экрана Android settings/другой Activity от crash.
4. Повторный Game→Exit→Launcher, минимум 10 циклов на каждом backend; Native и ANGLE запускать отдельными fresh процессами, соблюдая backend process lock. Выбор backend в UI без launch не должен падать; диалог Restart required является ожидаемым поведением.
5. Отдельно HOME, Recents, screen lock и возврат через ту же карточку; не создавать повторный MAIN поверх живой игры. Проверить sound/input/console и видимый мир. ESC111 закрывает консоль; перед screenshot/measure проверить отсутствие overlay.
6. Для сбоя собрать Java throwable / tombstone / ApplicationExitInfo / ANR trace доступными read-only средствами, terminal states/PID и engine/app log. На Android process death может произойти без onDestroy: marker — повод исследовать, а не доказательство типа.
7. В конце вернуть точный config/UI и проверить saves; между блоками остановить игру и погасить экран. Не менять thermal policy, яркость, global stay-on или системные refresh limits.

### Классификация результата

- **Java exception:** matching package/PID + `FATAL EXCEPTION` и stack trace, exception type/cause. CrashReporter слово не заменяет stack.
- **Native crash:** matching package/PID/TID, fatal signal/tombstone, symbolization candidate Build IDs. SIGTRAP может быть engine assert/watchdog; это требует trace, не предполагать SIGSEGV.
- **ANR:** ActivityManager ANR/exit reason и main/native thread stacks. Не считать любой долгий exit ANR без evidence.
- **Process death:** ApplicationExitInfo/PID starttime transition/low-memory evidence; отсутствие onDestroy ожидаемо при ряде OS kills.
- **Activity transition:** PID жив, корректная stopped/destroyed Activity и task stack; изменение экрана не равно process crash.
- **False marker:** marker есть, прежняя GameActivity/PID ещё живы, matching fatal evidence нет; ранее такой случай уже был.

## Полезное инструментирование отдельного кандидата

Bounded lifecycle records: monotonic timestamp, PID/TID, identityHashCode(Activity), taskId, event, finishing/destroyed/changingConfigurations, current/effective backend, SDL thread alive/currentState и setup generation. UI actions писать как resource ID/type без пользовательского path/custom-argument содержимого. Init/shutdown begin/end должны позволять связать SDLMain completion, nativeQuit и Launcher resume. Логировать только переходы и реальные пользовательские действия; никаких per-frame probes. Нужен append-mode для Launcher resume, чтобы ротация не уничтожила предыдущую crash evidence. Default exception handler не подавлять.

Mock lifecycle tests помогают доказать локальные контракты регистрации и порядка, но не доказывают Android OEM scheduling, JNI/GL lifetime или отсутствие ANR. Эти слова нельзя заменять фактом успешной сборки.

### L05: реализованное редкое lifecycle-инструментирование

`android/app/src/main/java/org/openxray/LifecycleTrace.java` добавляет ровно одну structured запись на каждый `onCreate/onStart/onResume/onPause/onStop/onDestroy` в обеих Activity. Содержимое: process sequence, `SystemClock.uptimeMillis`, PID/TID, Activity class + identity hash, task ID, имя события, finishing/changingConfiguration. Нет пользовательских путей/arguments, per-frame работы, таймеров, native engine callbacks или UI переключателя. Logger не меняет lifecycle state; каждое прежнее superclass событие остаётся вызванным один раз. Reentrancy guard предотвращает рекурсивную запись; RuntimeException/LinkageError необязательной диагностики поглощаются. Катастрофические VM errors вроде OutOfMemoryError не обещаны безопасными. Лог перехода перед `super.onDestroy` свидетельствует о входе в teardown, а не о завершении native shutdown; для последнего читать отдельные FT shutdown markers и engine logs.

`D/launcher/run_lifecycle_trace_tests.py` компилирует сам helper и извлекает фактические hook calls из Activity; внешние Android framework/Activity/AppLog API замокированы. **100 mock циклов**, 200 различимых Activity instances, 2000 transition records, оба порядка возврата, повторные pause/stop/start/resume; дополнительно 400 записей из четырёх host-потоков проверяют уникальность process sequence. При конкурентной доставке порядок строк не обязан совпадать с sequence: сортировать/сопоставлять по seq и monotonic timestamp, не делать вывод о main-thread order по физическому порядку file writes. Reentrant sink, RuntimeException, LinkageError, null Activity/state failure и последующая нормальная запись проверены. Все 12 hooks присутствуют по одному; все 12 super вызовов сохранены по одному. Это **не** 100 запусков игры/SDL и не device scheduling test.

Независимый source patch: `D/launcher/patches/L05-lifecycle-trace.patch`; результаты/hashes — `D/launcher/L05-results.json`. Для разделения L04/L05 сохранены два снимка Activity до инструментирования в `D/launcher/L05-before/`.

### Готовый offline план приёмки

`D/launcher/plan_launcher_cycles.py` по умолчанию/с `--plan` выводит только JSON, не запускает subprocess/ADB и не пишет файлы. Он **не имеет `--run`**. `--restore-result PATH` читает локальный restore JSON и проверяет success flags, config hash, 28 UI states и 12 save checks, но всё равно требует свежего root readback: локальный файл не доказывает текущее состояние планшета. Девять plan/restore gate self-tests PASS.

Снимок плана: `D/launcher/L05-device-cycle-plan.json`. В нём mandatory thermal restore **до APK**, Native и ANGLE отдельными fresh process группами по 10 циклов `Launcher→Game→normal Exit→Launcher→изменить/вернуть sensitivity→Game→normal Exit→Launcher`, visual/PID gates, HOME/Recents/lock smoke, сохранность config/UI/saves, evidence и stop conditions. Нельзя считать принудительный process restart между backend группами проверкой normal Exit. Root-оператор реализует реальные UI действия после подключения, поскольку координаты/видимость кнопок без устройства недостоверны. Не менять системный stay-on/thermal settings; между блоками package stopped/screen OFF.

## Артефакты и повтор тестов

`work/stability-20261023/launcher/run_launcher_tests.py baseline` создал исходные snapshots и baseline probes. После изменения исходников использовать уникальный режим (`candidate1`), **не переписывать baseline**. `run_crash_classifier_tests.py baseline/candidate1` — classifier fixtures. JDK `work/toolchain/jdk-17.0.20.1+1`; запуск bundled Python. Полные compile/run logs и JSON лежат рядом; ни один helper не импортирует или запускает ADB. APK/build и результаты других аудитов интегрирует root; аппаратный статус pending.

`run_resume_logging_tests.py baseline/candidate1` проверяет append-only file contract; `run_logging_lifecycle_order.py baseline/candidate-final` — оба порядка возврата Activity. `finalize_evidence.py` создаёт четыре независимых небольших patch файла в `D/launcher/patches/` и `final-evidence.json` с **raw byte SHA256** baseline/current source и patch hashes. В первоначальных source-probe JSON `sources` хэшируют нормализованный UTF-8/LF текст, поэтому их hashes не подменяют raw byte manifest при CRLF-файлах. L04 содержит новую AppLog функцию, одну строку Launcher.onResume и append-reopen после закрытия в OpenXRayActivity.onDestroy; изменения Launcher config принадлежат root и в этот patch не включены. Во всех отчётах host PASS отделён от pending hardware; окончательную APK/build verification добавляет root.
