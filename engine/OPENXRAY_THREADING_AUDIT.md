# OpenXRay Android: аудит потоков и CPU

Исследование D3, 23–24 октября 2026. **Серьёзное расширение параллелизма сейчас преждевременно.** Существующий pool уже перекрывает часть подготовки с рендером, но общая очередь имеет известные дефекты, а ряд подсистем сохраняет неявную последовательность через общие данные. Сначала нужны корректность зависимостей, измерение critical path и небольшие устранения лишней работы. Ни улучшение FPS, ни снижение мощности/температуры в D3 не измерялись. Основания: [D2 очередь][d2queue], [новый source audit pool][queue], [gameplay map][game], [render map][render].

Рабочий R: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build. Изолированное исследование D3: соседний work/threading-cpu-20261023. Новые файлы вне R — notes, модели, микробенчмарки, raw logs и планы. В R добавлены только шесть Markdown-отчётов этого этапа. Код, renderer, scheduler, зависимости, settings, APK и сейвы не изменены; ADB не выполнялся.

## Проверенный исходный checkpoint

Прочитаны PROJECT_STABILITY_AUDIT, STABILITY_FOLLOWUP_RESULTS, NEXT_STABILITY_SESSION, NEXT_THERMAL_OPTIMIZATION_SESSION, предыдущие thermal/performance reports и реальные результаты. D2 source hashes сверены до новой работы; актуальные11735 файлов сохранены в D3/source-baseline.zip,53477731 байт, SHA256 cb11d24818846cb3e427c88ba802abac1c69f94134cbeeb178668ff2ee988922. Нельзя восстанавливать Git HEAD поверх этого дерева. [Манифест][baseline].

Git прочитан без optional locks и без изменения глобальной конфигурации: HEAD28e6ff87203f0b93bf1935c9692b282d605b7072; tracked diff58 файлов,1529 additions/240 deletions, status84 строки включая untracked. Это предшествующая работа, не результат D3. Изначальный ownership отказ обойдён только точным command-local safe.directory для чтения своего репозитория; системные Git settings не менялись. [Сохранённые status][gitstatus], [diff-stat][gitdiff].

cordonperf1, thermala1, stabilityaudit1 проверены по прежним SHA256. D1/D2 candidates и frozen stage не подменялись. Восстановление thermal-настроек устройства по-прежнему pending; никаких сведений о сегодняшнем состоянии планшета нет. [APK preflight][apk], [thermal handoff][thermalnext].

## Фактическая топология

«Главный» ниже — native SDL/game/render thread, а не Java UI Looper. Task — единица работы, не отдельный системный поток. Worker lane — роль; waiting main также может исполнять tasks. Это следует из [SDLMain][sdl], [ProcessFrame][frame] и [TaskManager::Wait][wait].

| Поток / роль | Создание и выполнение | Что нельзя предполагать |
|---|---|---|
| Android UI/main Looper | Activity lifecycle, views, touch overlay, refresh-rate request; SDL запускает отдельный SDLThread | Наличие UI thread не означает отдельный engine render thread. |
| SDLThread / native game main | SDLActivity создаёт SDLMain; Java задаёт THREAD_PRIORITY_DISPLAY, затем nativeRunMain; CApplication event loop → Device.ProcessFrame | Реальный nice/cpuset/core placement проверяется на устройстве, не вычисляется из названия приоритета. |
| Task workers | TaskManager registers caller и создаёт hardware_concurrency−1 threads через Threading::RunThread. Старые Tab S9 логи сообщают8 логических CPU, следовательно обычная конфигурация — caller+7 workers | Это не8 одновременно полезно работающих ядер. Нет heterogeneity-aware budget или измерения oversubscription. |
| Aggregate frame task | Один AddTask выполняет все seqParallel delegates **последовательно**, затем seqFrameMT.Process; main в это время делает DoRender | Это не task-per-NPC/AI/physics. Задачу может забрать waiting caller. |
| Render preparation jobs | HOM, Details, main/sun/rain/light CPU preparation используют pool и собственные join sites | CPU command-list/context ID не EGL context. GL submission остаётся main. |
| Engine watchdog | device.cpp::ft::install создаёт pthread; D1 добавил stop/join/restart и ранний FT_SHUTDOWN | Старое описание «навсегда detached watchdog» из thermal audit уже историческое. Checkpoint formatting/trail mutex в release остаются. |
| Java data preparation | OpenXRayActivity::startGameDirectoriesAsync создаёт GameDataInitThread, затем UI completeGameInit | Это startup assets/config preparation, не постоянный streaming pool. |
| Java log capture | AppLog::startLogcatCapture создаёт thread + logcat subprocess; reader ждёт pipe, flush каждой строки | Не весь elapsed этого потока является CPU. Рост сообщений может увеличить работу, но вклад не измерен. |
| Audio device/mixer | Реальные старые логи: OpenSL; bundled OpenAL backend имеет mixerProc/thread start/join и Android callbacks | Game sound update, streaming prefill и mixer — разные роли, не один «audio worker». |
| ANGLE internal | Pinned source содержит ANGLE-GC recycling thread и feature-dependent shader/worker infrastructure | Нельзя автоматически считать X-Ray culling/command preparation перенесёнными на эти threads. Native driver internal threads закрыты/не измерены. |
| Загрузочные/условные threads | CDB::MODEL::build при -mt_cdb создаёт CDB-construction; splash thread join при HideSplash; network-time-sync, configs dumper, GameSpy refresh зависят от feature/сценария | Наличие source branch не означает живой thread в одиночной игре. Framework Binder/ART/GC/driver/SDL helpers инвентаризировать по реальной трассе. |

Создание: [SDLMain][sdl], [ThreadUtil][threadutil], [pool][poolsource], [watchdog][watchdog], [data init][datainit], [logcat][logcat], [CDB build][cdb], [splash][splash]. Полная source inventory и условные ветви собраны в [D3 notes][queue] и D3/thread-creation-sites.txt; состав runtime threads сейчас не снимался.

## Карта подсистем

| Подсистема | Текущий execution и синхронизация | Shared state / вывод для параллелизма |
|---|---|---|
| Game objects | FrameMove→seqFrame→CLevel/IGame_Level→CObjectList::Update. Parent SingleUpdate вызывается прежде child, затем общие PostUpdateCL | Object registries, inventory, HUD, destruction/relcase/spatial state. Нет безопасного independent UpdateCL loop. |
| Game scheduler | CSheduler::Update синхронно выполняет realtime/due scheduled objects на main | Это отдельный serial scheduler, не TaskManager. Порядок/self-unregister важны. |
| AI/ALife | Vision dispatch отключён через false&&, независимо от saved mt_ai_vision. Часть path/object/ALife work — delegates в aggregate task | CGraphEngine имеет общие scratch/algorithms, пять ScopeLock закомментированы; path query также меняет restriction borders. Самостоятельные parallel path jobs небезопасны без изоляции. |
| Lua | lua_newthread/lua_resume — корутины общей VM. Обычные updates main; GC при mtLUA_GC может быть aggregate delegate | VM map lock защищает registry lookup, не исполнение Lua. Нельзя распределить coroutines по OS workers без новой модели VM/bindings. |
| Физика | Default psDeviceFlags содержит mtSound/mtNetwork, **не mtPhysics**. CPHWorld при false регистрируется в main seqFrame; true переносит callback целиком в seqFrameMT | Collision/tune/solver/data/spatial phases, contacts, callbacks и ragdolls связаны. Независимость островов ещё надо доказать. |
| Анимация/скелет | CKinematics::CalculateBones вызывается из разных consumers, но глобальный UCalc_Mutex сериализует расчёт | Внутри tracks/callbacks/physics callbacks/bounds/Random. Это не чистое перемножение матриц. Полезный будущий split: immutable input → private transforms → ordered commit. |
| CPU vertex skinning | ARM64 Skin4W использует ParallelFor даже для малого vCount; часть иных parallel bone ветвей отключена macro | Возможен measured serial threshold с прежней арифметикой. Нельзя выводить bottleneck без количества вызовов/vCount и CPU trace. |
| Audio | Main update emitter/listener; aggregate SoundRenderer; prefill tasks с ожиданием; отдельный OpenSL mixer | CON009 publication/lifetime остаётся prerequisite. Дробление prefill может увеличить overhead/underruns. |
| Resources / FS | OGL DeferredUpload serial; Load смешивает FS read, GLI/DXT CPU decode и GL upload | Вынести можно только bounded decode из immutable bytes, сохранив upload на GL owner. FS lock_rescan — счётчик, не общий mutex; r_open способен менять index. |

Все точные entrypoints, строки, ownership и тестовые требования: [gameplay notes][game]. Skin4W/active build flags: [pool research][queue]. Существующие fixes quest/physics/audio/cadence не отменялись и не считаются новыми оптимизациями.

## CPU bottlenecks: что подтверждено и чего нет

Исторический FHD Native frozen4: render-thread CPU median15.073177ms (1375 frames), GPU inclusive render mean17.404284ms (43 samples), около4709 draw submissions/frame. Это показывает существенную стоимость обеих частей, **не доказывает**, что весь кадр CPU-bound или что дополнительные workers дадут выигрыш. Полная simulation/worker CPU и критический путь не измерены. ANGLE smoke не строгая A/B пара. [Raw-backed index][history], [render telemetry][render].

Старые thermal controls на cordonperf1 дали Native unlocked46.053FPS, Native cap60≈45.956FPS, ANGLE cap60≈40.726FPS. Разные thermal starts/окна, никаких measured W/J; candidate thermala1 D прерван и исключён. Разность FPS не является разностью CPU overhead backend. Process ticks86.62/87.44/89.67CPU-seconds охватывают **внешние endpoints**, не точное SF окно: из них нельзя получить корректные CPU/frame. [Проверенные исходные inputs][history].

Конкретные waits: TaskManager::Wait может выполнять полезные jobs или активно проверять пустые queues; idle workers паркуются Event. GLES map/upload/swap могут ждать driver/GPU; fallback R_sync_point::Wait пропускается при effective -gles_stream_upload/-gles_stream_sync. FPS limiter отдельно Sleep, uses integer ms и paused/scaled TimerGlobal; существующий thermal pacer prototype не интегрирован. Нельзя назвать всё wall-время main вычислением. [Frame map][framemap], [render waits][render], [pool loop][wait].

## Новые findings, не production fixes

| ID / confidence | Доказательство | Статус |
|---|---|---|
| CPU-001 / Confirmed source mismatch | CRender::Calculate сравнивает vCameraDirectionSaved с vCameraPosition перед detect_sector. Обычно повторяет запрос при неподвижной камере | Isolated conservative gate:7 scenarios/100000 deterministic frames, ASan/UBSan/NDK и4 mutations. При stationary10000→1 query в модели; не ms/FPS/energy. Простая замена поля неверна на origin/reload/slow crossings. |
| CON-NEW-01 / Strong source evidence | HOM и Details dispatch независимы; Details читает HOM/Raster до собственного producer barrier. SSA globals пишутся main после dispatch Details | Static partial-order model выявляет3 unordered conflicts. Worker overlap или main helping делает такой порядок достижимым. Не runtime race-detector result и не новая установленная причина crash. |
| CON-NEW-02 / Strong source evidence | HOM render/visible, detail visibility/decompress используют общий non-TLS Random с volatile, не atomic state | Одного H→D edge недостаточно: main culling/frameMT также может использовать RNG. Нельзя заменить на TLS RNG без проверки последовательности/детерминизма. |
| CON-NEW-03 / Confirmed isolated API hazard | ParallelForEach wait=false захватывает Function по ссылке; ASan фиксирует stack-use-after-scope | Реальных активных false callers не найдено. Риск будущего расширения API, не диагноз текущей игры. |
| CON-NEW-04 / Lifecycle suspicion | First-frame-after-reset early returns могут пропустить обычные HOM/Detail joins; frame-end join ждёт другую aggregate task | Отдельно проверить generation/quiescence; UAF/crash не воспроизведён, production patch не делался. |

Доказательства/ограничения: [render notes][render], [независимое review][review], [queue notes][queue]. Новые render graph edges являются моделью порядка, не готовым C++ happens-before исправлением: нужно исправленное acquire/publication и lifetime TaskQueue. Предыдущий D1 zero-CPU underflow также уточнён: normal constructor сначала попадает в активный assertion; его нельзя повторять как доказанный обычный путь. Single-worker self-steal и отсутствие graceful0CPU policy остаются. [D3 queue][queue].

## Решение

До серьёзного parallelism: корректная публикация/завершение/lifetime/full/shutdown очереди, HOM/Details/SSA/RNG dependency review и реальная per-thread трасса. Затем выбирать только чистые snapshot-based kernels с private outputs и детерминированным merge. Не запускать GLES на pool, Lua/AI/physics bulk jobs или новый worker count по предположению. [Ranking][ranking], [profiling plan][profiling].

Первый небольшой кодовый эксперимент после измерения — **serial camera-sector gate** с явной topology generation/invalidation. Он не создаёт tasks и не зависит от mutex replacement, но его safety и вклад ещё требуется проверить реальными CDB/lifecycle/device tests. Если trace покажет, что запрос ничтожен, перейти к measured Skin4W grain или wait/driver costs, а не считать gate большим FPS-исправлением. [Prototype/review][review].

[baseline]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/baseline-manifest.json
[gitstatus]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/git-status.txt
[gitdiff]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/git-diff-stat.txt
[d2queue]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/stability-followup-20261023/taskqueue/TASKQUEUE_CANDIDATE_AUDIT.md
[queue]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/taskqueue/TASKQUEUE_CPU_RESEARCH.md
[game]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/gameplay/GAMEPLAY_THREADING_NOTES.md
[render]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/render/RENDER_THREADING_NOTES.md
[review]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/review-render-findings.md
[history]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/historical-evidence-index.json
[apk]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/android/APK_PROFILING_PREFLIGHT.md
[thermalnext]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/NEXT_THERMAL_OPTIMIZATION_SESSION.md
[sdl]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/android/app/src/main/java/org/libsdl/app/SDLActivity.java:1899
[frame]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrEngine/device.cpp:972
[threadutil]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrCore/Threading/ThreadUtil.h:37
[poolsource]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrCore/Threading/TaskManager.cpp:147
[wait]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrCore/Threading/TaskManager.cpp:316
[watchdog]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrEngine/device.cpp:532
[datainit]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/android/app/src/main/java/org/openxray/OpenXRayActivity.java:395
[logcat]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/android/app/src/main/java/org/openxray/AppLog.java:379
[cdb]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrCDB/xrCDB.cpp:47
[splash]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrEngine/x_ray.cpp:548
[framemap]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/FRAME_EXECUTION_MAP.md
[ranking]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/THREADING_CANDIDATE_RANKING.md
[profiling]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/ANDROID_CPU_PROFILING_PLAN.md
