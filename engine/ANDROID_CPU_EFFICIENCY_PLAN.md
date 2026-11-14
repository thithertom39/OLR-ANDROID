# CPU-эффективность Android: что проверять и что пока не менять

D3,23–24.10.2026. **Больше parallel tasks не равняется меньшей энергии.** На этом форке сначала следует отделить полезные вычисления от spinning, driver CPU, GPU/display waits и FPS-cap sleep. Переходить к распределению работы между ядрами разумно только при измеренном CPU critical path и исправленных ownership/queue/dependency контрактах. [Карта кадра][frame], [pool audit][pool], [render audit][render].

## Исходные факты

- Snapdragon8Gen2 — heterogeneous платформа; сохранённый engine log сообщает8 logical CPUs. Текущий pool использует caller+hardware_concurrency−1 workers, без учёта различной производительности ядер, доступного cpuset, частот и других runtime threads. Фактические capacity/core residency/доступные CPU сегодня не снимались. [Build/runtime evidence][pool].
- SDLMain уже задаёт Android THREAD_PRIORITY_DISPLAY. POSIX ThreadUtil GetPriority возвращает normal как заглушку, SetPriority ничего не делает; это **не измерение фактического nice** у native workers. Thread placement/приоритет/CPU time требуется читать из трассы. Менять nice/affinity/root/frequencies в этом этапе не надо. [SDLMain][sdl], [ThreadUtil.cpp][prio].
- Текущая source integration ADPF/PerformanceHint/Thermal APIs не найдена. Есть refresh-rate vote при unlock; это запрос compositor, не CPU optimization и не гарантированный FPS. [APK/API preflight][apk], [refresh request][refresh].
- Старые thermal данные не содержат измеренных watts/joules. AP vendor values имеют неизвестные units; USB battery current не полная мощность устройства. Итог кандидатных A-правок thermala1 по энергии ещё не принят. [Historical evidence][history], [thermal handoff][thermal].

## Когда parallelism может помочь энергии

Это инженерная модель, не измерение Tab S9: если достаточный объём чистой работы укладывается на нескольких ядрах при более низком operating point и completion сохраняет тот же deadline/cadence, суммарная энергия иногда уменьшается. Если ускорение лишь повышает uncapped FPS, активирует дополнительные ядра, копирует snapshots, добавляет mutex/atomics/merges и память, мощность может вырасти. Короткий burst и последующий idle также не гарантируют экономию при иной DVFS/thermal policy. Требуется одинаковый **полезный результат и число представленных кадров**, а не одинаковый процент одного ядра. [Проектные зависимости][game], [измерительный план][profile].

Время кадра ограничивается critical path, не суммой всех thread times. Для планирования полезна модель T≈serial+parallel/P+dispatch/barrier/merge, но она игнорирует bandwidth/cache/разные ядра/driver overlap. Энергия зависит от суммы активностей CPU, GPU, memory, display и прочего устройства. Факт меньшего wall-time kernel не доказывает меньше J/frame. [Perfetto power limits](https://perfetto.dev/docs/data-sources/battery-counters), [старые контрольные данные][history].

## Приоритет: устранение лишней работы

1. **Измерить TaskWait running CPU и пустой helping loop.** Idle workers уже спят на Event; заменить helping Wait простой sleep нельзя: незавершённая nested работа/SDL event pump могут зависнуть. Нужны target completion generation/notification, acquire и no-lost-wake protocol. [TaskManager::Wait][wait], [pool prerequisites][pool].
2. **Serial camera query gate.** Устранить повторный запрос только при exact совпадении query position и явной geometry epoch; first/invalid/reset обязаны повторяться. Прототип даёт call-count proof в synthetic модели, не энергию. Это меньше риска, чем добавление потоков. [Render prototype/review][review].
3. **Измерить grain на реально активном Skin4W.** Не распылять малые vertex ranges на дерево tasks; при подтверждённом overhead оставить прежний vertex loop для малого объёма. Threshold выбирается на ARM из данных, не из WSL. [Новый44-case fixture][pool].
4. **Сократить повторную diagnostics/API/matrix работу с прежним результатом.** Listener zero-velocity и dead point-light jitter уже удалены в thermala1 — повторно не предлагать как новый gain. Exact OpenAL properties cache, transform batching и immutable luminance geometry остаются отдельными кандидатами с invalidation/ownership gates. [Thermal audit][thermalreport], [ranking][ranking].

## Pool и память

Текущее разбиение ParallelFor создаёт binary tree: 2L−1 tasks при L листьях. Новый source-header fixture подтверждает, например,29 tasks на31 элемент при8 logical workers. Каждый PushTask вызывает общий Event::Set; это дополнительный mutex/signaling путь, но live доля неизвестна. Synthetic Event test без waiters измеряет только uncontended signalling, не wake latency и не рабочий pool. [Новые результаты][pool].

Task на ARM64 рассчитан на128bytes, TLS arena4096slots — номинально512KiB task storage на использующий её thread, до прочих buffers. Это **не измеренная resident memory**. Queue head/tail и shared parent jobs/stats могут создавать cache contention; отсутствие доказательства padding/alignment не равно измеренному false sharing. Нужно измерить contention/task counts/allocations и при наличии PMU cache counters, иначе оставить гипотезой. [Task.hpp][task], [TaskManager storage][storage].

Дальнейший worker-budget эксперимент допустим только после correctness gates: отдельные варианты с1/2/4/7 **дополнительными OS workers** плюс main, фиксированный foreground workload/cap, без affinity. Не путать это с1 logical worker: текущий self-steal/zero CPU paths не готовы. Уменьшение pool может увеличить критический путь и время активности; расширение увеличивает interleavings и memory pressure. [D2 correctness][d2], [D3 уточнения][pool].

## Android framework: применимость

| Механизм | Что он даёт | Граница этого проекта |
|---|---|---|
| Обычное Android scheduling/DVFS | OS выбирает placement/частоты в рамках своей policy | Не закреплять threads на big cores, не переписывать частоты/thermal policy. Сначала trace migrations/runnable/frequency/idle. |
| Performance Hint session | Сообщение periodic workload/target duration для группы своих long-lived threads; система может выбирать operating point | Не автоматическое энергосбережение. Нет реализации. Не создавать session на каждый job или на чужие GPU-driver threads. |
| Power-efficiency preference | Advisory preference API35 | На текущем SDK34 Java jar метода нет; сам NDK27 header не означает availability на устройстве. Future dynamic/API guard и device acceptance обязательны. |
| Thermal status/headroom | Сигнал thermal pressure/прогноз, а не потребляемая мощность | Начать с monitor-only; NaN/unsupported не превращать в0. Headroom читать из одного места не чаще10s. |
| Frame pacing | Уменьшает queue stuffing и неравномерную подачу при корректной интеграции | Не добавлять Choreographer/Swappy поверх SDL вслепую; surface/context/backend lifecycle и независимая проверка cadence. |
| FPS cap/display vote | Ограничивает произведённую работу или запрашивает display rate | Сравнивать одинаковые target cap и реальную cadence. Cap ниже требуемого или отключение AA не оптимизация по задаче. |

API факты проверены по [официальной Java Session reference](https://developer.android.com/reference/android/os/PerformanceHintManager.Session), [NDK reference](https://developer.android.com/ndk/reference/group/a-performance-hint), [Thermal API](https://developer.android.com/games/optimize/adpf/thermal), [Frame Pacing guide](https://developer.android.com/games/sdk/frame-pacing) и установленным SDK/NDK в [preflight][apk]. Это предложения для последующего исследования, не изменения APK.

Java base hint API начинается31; установленный native C header помечает base33, setThreads34, WorkDuration2/preferPowerEfficiency35. При minSdk24 нельзя добавить обязательные high-API imports без корректной загрузки/availability. Сейчас compile/target34; SDK/NDK не обновлялись. Поддержка service/HAL и выгода на текущем Samsung неизвестны. [Local symbol proof][apk].

Для будущего session измерять устойчивую периодическую critical work duration с согласованной clock base. Не выдавать сумму CPU всех workers за wall duration и не подставлять паузу limiter в полезные вычисления. GPU duration/total/CPU различать; API35 WorkDuration поддерживает раздельные поля, старые API — иной контракт. Точные границы для этого overlapped engine потребуют review после трассы. Слишком маленькая target duration способна заставлять OS повышать performance без экономии. [Карта реального overlap][frame], [NDK work-duration contract](https://developer.android.com/ndk/reference/group/a-performance-hint).

Headroom предназначен для предотвращения ухудшения, но здесь не разрешено автоматически снижать resolution, foliage, shadows, effects, AI/physics cadence или менять баланс. Thermal мониторинг сначала нужен для **валидности/остановки теста**, затем для оценки возможных разрешённых стратегий при прежнем качестве. [Thermal restore/guards][thermal], [user constraints в ranking][ranking].

## Native GLES и ANGLE: CPU-вопрос

Оба пути принимают GL commands из main; ANGLE внутри выполняет validation/state/translation и формирует Vulkan work. Pinned e5640f4ec9ce5fec357186fd7fa40ad3030f2698 имеет внутренние recycling/worker paths; фактические feature flags и CPU доли не сняты. Vendor GLES тоже может иметь внутренние threads. Поэтому сравнивать надо main+pool+driver/ANGLE threads при одинаковых кадрах, не только main wall или один FPS. [Pinned-source analysis][render].

Без изменения renderer сейчас доступны исследование повторных state/uniform submissions, shader cache miss/compile timing, waits/maps/uploads и snapshots. Старый uniform grass batching уже принят, его gain нельзя присвоить новому этапу. Полноценный renderer переписывать не следует, пока trace не укажет именно CPU submission/driver bottleneck. [Performance history][render], [profiling plan][profile].

## Критерий решения

Успех будущего кандидата: тот же результат/настройки/cadence, меньше доказанно лишнего running CPU или измеренной энергии, без роста deadline misses/frametime tails и регрессий. CPU-time improvement — промежуточный результат; power benefit требует валидных power rails/external measurement или явно ограниченного общего battery опыта без зарядного искажений. Если источник мощности недоступен, вывод заканчивается CPU/work/cadence, а не «на X градусов холоднее». [Measurement protocol][profile], [power counters](https://perfetto.dev/docs/data-sources/battery-counters).

Первая аппаратная сессия: exact thermal restore → allowed system baseline → missing-stack decision → scoped instrumentation/candidate. Следующий шаг выбирается по измеренной доле, не по количеству найденных задач. [Next threading session][next].

[frame]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/FRAME_EXECUTION_MAP.md
[pool]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/taskqueue/TASKQUEUE_CPU_RESEARCH.md
[render]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/render/RENDER_THREADING_NOTES.md
[game]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/gameplay/GAMEPLAY_THREADING_NOTES.md
[apk]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/android/APK_PROFILING_PREFLIGHT.md
[history]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/historical-evidence-index.json
[review]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/review-render-findings.md
[sdl]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/android/app/src/main/java/org/libsdl/app/SDLActivity.java:1909
[prio]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrCore/Threading/ThreadUtil.cpp:171
[refresh]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/android/app/src/main/java/org/openxray/OpenXRayActivity.java:190
[wait]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrCore/Threading/TaskManager.cpp:316
[task]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrCore/Threading/Task.hpp:21
[storage]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrCore/Threading/TaskManager.cpp:64
[d2]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/stability-followup-20261023/taskqueue/TASKQUEUE_CANDIDATE_AUDIT.md
[thermal]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/NEXT_THERMAL_OPTIMIZATION_SESSION.md
[thermalreport]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/ANDROID_THERMAL_POWER_AUDIT.md
[ranking]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/THREADING_CANDIDATE_RANKING.md
[profile]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/ANDROID_CPU_PROFILING_PLAN.md
[next]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/NEXT_THREADING_SESSION.md
