# Кандидаты CPU-эффективности: рейтинг и условия

D3,23–24.10.2026. Порядок — **безопасность и проверяемость следующего шага**, не прогноз FPS/W. High likelihood относится к source overhead/опасности, а не к уже измеренному главному bottleneck Tab S9. Измеренных power gains нет. [Сводный аудит][audit], [исторические inputs][history].

## Нулевой этап: prerequisites, не performance release

До расширения concurrency закрыть TaskQueue publication/slot reuse/false-empty drain, acquire completion, task handle generation/lifetime, full allocator/queue policy и quiescent membership/shutdown. D2 mutex reference имеет большой host overhead и fatal full policy, поэтому не принят автоматически. Source HOM→Details/SSA/RNG dependencies и first-reset-frame jobs также требуют собственного correctness review. [D2][d2], [D3 pool][pool], [render review][review].

| Ранг / кандидат | Доказательства и CPU bottleneck likelihood | Энергия / FPS | Объём / риск | Независимая проверка / TaskQueue gate |
|---|---|---|---|---|
| 1. Exact serial camera-sector query gate | Confirmed direction-vs-position mismatch; query cost/частота в игре неизвестны. Вероятность лишних calls высокая, вероятность главного bottleneck неизвестна | Меньше query work возможно; ms/FPS/W не измерены | Малый/средний: topology epochs и callback/invalidation audit; низкий concurrency риск, средний correctness риск при неполном ключе | Уже standalone source-gate fixture+ASan/UBSan/NDK. Реальный CDB/origin/portal/reload test ещё нужен. **Новых tasks нет**, queue fix не предпосылка самой модели. |
| 2. Measured small Skin4W serial/grain | Actual ARM64 fallback использует default ParallelFor даже для малых ranges;44-case header fixture показывает мелкое дерево tasks. Active count/vCount ещё неизвестны | Возможны меньше wake/queue/copies/parent atomics; throughput может и ухудшиться при большом range | Небольшой threshold branch при прежней арифметике; output comparison/tail tests. Не угадывать threshold по x86 | Сначала trace, bitwise/tolerance-aware reference per vertex без изменения math, в том числе0/1/tails. Sequential bypass не лечит queue; расширение parallel path только после prerequisites. |
| 3. Completion-aware helping/parking | Busy loop в Wait подтверждён; доля пустого CPU не снята. Не все workers spin — idle Event уже паркует | Потенциально заметно при большом no-work Wait; FPS/latency может ухудшиться из-за wake | Средний/высокий: completion generation, no-lost-wake, nested jobs, SDL pump, shutdown | Детерминированные notify-before/after/wait, nested/dependent tasks, spurious wake, full/shutdown. **Сильная зависимость от queue/lifetime**. |
| 4. Измеренная granularity/batch scheduling | 2L−1 tasks и один Event::Set на PushTask подтверждены; live task histogram неизвестен | Возможны меньше active CPU/lock traffic; oversized batches могут ухудшить баланс | Средний: leaf-only/inline split и parent semantics, bounded preallocated outputs | Header model44 cases уже есть; actual scheduler надо отдельно. ASan/TSan/ARM stress + order/count/result equality. **Да**. |
| 5. Сократить repeated diagnostics/API updates | Release ft::note mutex+format, AppLog capture flush, OpenAL equal-state публикации существуют. CPU доля неизвестна; две thermal A правки уже выполнены | Возможная экономия постоянной CPU/IO работы, особенно при многословных логах; нового эффекта не измерено | Малый/средний; нельзя терять heartbeat/background/crash evidence или sound commit | Recorded-call/state equivalence, context restart и crash-log durability. No queue requirement для serial cache, отдельный diagnostics policy review. |
| 6. Transform batch + immutable luminance geometry | Thermal B inventory: повторные matrix products и1344 одинаковых bytes/3 stream ranges; work already studied, не реализован | Возможны меньше CPU math/upload/driver work; неизвестная доля frame/energy | Малый/средний, interim-state observation и VBO ownership/reset риск | Не повторять прежние8192 byte tests; новые invalidation/lifetime fixtures и driver trace. **Нет**, если serial и owner GL сохранён. |
| 7. Bounded CPU-only texture decode stage | OGL DeferredUpload serial и смешанный FS/decode/upload path подтверждены; loading stalls, не обязательно sustained CPU bottleneck | Возможны меньше main stalls; одновременно больше memory/active cores и тот же total work | Средний: immutable input/output packet, byte budget, cancellation/generation, main GL upload | Синхронное сравнение всех mip/format bytes, bounded allocations, errors/unload/context loss. **Да** для pool; FS/codec stability prerequisites тоже. |
| 8. Pure animation transforms snapshot | Global bone mutex доказан; no evidence of dominant bone-runningCPU yet | Возможное critical-path сокращение только после выделения чистого kernel; энергия неизвестна | Высокий: tracks/events/physics callbacks/bounds/Random нужно оставить ordered commit | Exact pose/event/bounds comparison, fixed inputs, spawn/death/ragdoll and CPU trace. **Да**, плюс explicit world snapshot/lifetime. |
| 9. Pure culling/classification snapshot + stable merge | Disabled static traversal/maps и shared visual markers показывают отсутствие готовой независимости | Потенциал при measured prepare bottleneck; snapshot/merge/alloc может поглотить gain | Высокий: per-worker outputs, deterministic sorting/duplicate suppression/transparency | Сравнение exact visible/draw order lists/callback counts на reference frames; jobs не трогают GL/world. **Да**, HOM/SSA snapshot prerequisite. |
| 10. Worker budget и ADPF hints | Pool budget homogeneous; API integration отсутствует. Oversubscription/OS benefit — гипотезы | Может помочь J/frame при той же cadence либо усилить нагрев/пропуски | Средний; API availability, session lifecycle, pool-indexed state; без affinity/priority forcing | Matched-cap CPU/power/thermal traces, device-supported hint session only. Budget changes **после queue/startup fixes**; hints сами correctness не решают. |

Конкретные исходники/функции и риски каждой строки: [camera/render][render], [skinning/pool/Wait/Event][pool], [animation/AI/physics/resource loading][game], [thermal B sources][thermal]. Официальные API/Android constraints: [CPU efficiency plan][efficiency].

## Почему первая ставка — не AI и физика

AI paths используют общий CGraphEngine scratch и restriction mutation; vision queries читают live objects/skeleton/spatial database. Physics step выполняет связанные collision/tuning/island solve/data updates; Lua callbacks и RNG мешают независимости. Дополнительные задачи без новых snapshots меняют порядок и детерминизм. Нет измерений, что именно эти serial kernels ограничивают тяжелую сцену Кордона. Их стоимость и самый безопасный independent subkernel сначала должен показать profiler. [Gameplay proof][game].

Возможный будущий AI experiment — immutable bounds/frustum math с ordered result merge, **не** parallel shedule_Update/pathfinder/VM. Для физики сначала доказать независимые islands после формирования contacts, private scratch/events, deterministic commit. Это большие проекты; сейчас они проигрывают более узким serial/wait/grain кандидатам. [Game dependencies/candidate tests][game].

## Что дают новые микропроверки

- Sector model:7 сценариев +100000 generated frames; stationary10000→1 query, состояния/уведомления совпадают с always-query static-topology reference. Исправленный reference намеренно отличается от buggy baseline при direction==position. Нет CDB/GPU/device gain proof. [sector-gate-results][sector].
- ParallelFor:44 source-header coverage/task-count cases, sanitizer PASS;31 элемент/8 workers→29 Tasks. Это proof overhead structure, не время production task. ARM64 компилируется stand-in fixture, не новая engine TU/APK. [pool evidence][pool].
- Event Set:8 samples на batch1/8/32/128, x86 WSL median7.79/1.11/0.456/0.296ns **на synthetic item**, где событий становится меньше. Нет waiters/work stealing, значит это не измерение ускорения wakeup/pool и не основание просто удалить сигналы. [raw Event results][event].
- ASan выявил ForEach async borrowed functor lifetime, но активных wait=false callers нет. D2 race/stress/mutex tests не повторялись. [queue research][pool].

## Минимальный следующий эксперимент

1. После exact thermal restore снять system per-thread trace и счётчик/стоимость camera-sector query; не добавлять новые workers.
2. Если доля оправдывает изменение, отдельный serial gate candidate: exact position+geometry epoch, обязательные first/invalid/reload/reset invalidations, прежние sector-change callbacks/minimap. Не ограничиваться rename Direction→Position.
3. Проверить реальные static CDB/portal transitions, малые шаги, origin, teleport, changing level, vid_restart/HOME, invalid-sector retry и обе backend. Никакого смешивания с queue/EGL/mod candidates.
4. При незначимой стоимости запроса оставить prototype и перейти к измеренному Skin4W либо Wait/driver path. Выбор rank1 означает низкий объём/независимость, **не обещание максимального FPS gain**. [Review][review], [hardware plan][profile].

## Критерии приёмки будущих кандидатов

Та же сцена/положение/качество/cadence; меньше relevant runningCPU/commands/allocs или реальных J/frame за пределами разброса; tails не ухудшились; no visual/gameplay/audio/lifecycle regression. Отсутствующий energy counter означает unknown, а не0. Не снижать density/distance/AA/shadows, AI budgets/physics cadence или FPS target для отчётного выигрыша. [Measurement protocol][profile].

[audit]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/OPENXRAY_THREADING_AUDIT.md
[history]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/historical-evidence-index.json
[d2]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/stability-followup-20261023/taskqueue/TASKQUEUE_CANDIDATE_AUDIT.md
[pool]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/taskqueue/TASKQUEUE_CPU_RESEARCH.md
[render]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/render/RENDER_THREADING_NOTES.md
[review]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/review-render-findings.md
[game]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/gameplay/GAMEPLAY_THREADING_NOTES.md
[thermal]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/ANDROID_THERMAL_POWER_AUDIT.md
[efficiency]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/ANDROID_CPU_EFFICIENCY_PLAN.md
[sector]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/render/sector-gate-results.json
[event]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/taskqueue/event-amortization-results.json
[profile]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/ANDROID_CPU_PROFILING_PLAN.md
