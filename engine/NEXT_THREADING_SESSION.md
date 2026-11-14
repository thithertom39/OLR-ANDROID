# Продолжение CPU/threading исследования OpenXRay Android

Checkpoint D3, начат23.10, завершение24.10.2026. **Весь этап изолирован. Production code/APK/settings/dependencies не менялись, ADB не выполнялся.** Ни серьёзное распараллеливание, ни экономия мощности не объявлены выполненными. Новый текущий stage указан в D3/session-state.json; integrity в D3/source-delta-final.json и evidence-sha256.json.

W = C:/Users/User/Documents/Codex/2026-10-11/gjvyb. R = W/work/openxray-build. D3 = W/work/threading-cpu-20261023. Не переименовывать D3 при смене календарной даты: это имя начатой сессии, оно уже записано в manifests. D1/D2 и thermal evidence сохраняются в прежних каталогах.

## Прочитать сначала

1. [OPENXRAY_THREADING_AUDIT.md][audit] — topology, confirmed/hypothesis, constraints.
2. [FRAME_EXECUTION_MAP.md][frame] — main/aggregate/render overlap и пять разных timings.
3. [THREADING_CANDIDATE_RANKING.md][ranking] —10 конкретных кандидатов и TaskQueue gates.
4. [ANDROID_CPU_EFFICIENCY_PLAN.md][efficiency] — Android APIs/power interpretation.
5. [ANDROID_CPU_PROFILING_PLAN.md][profile] — future hardware protocol и tested SQL.
6. Прежние NEXT_STABILITY_SESSION и NEXT_THERMAL_OPTIMIZATION_SESSION — не заменены этим исследованием; pending restore/production High остаются.

## Точный checkpoint и сохранность

D3/baseline-manifest.json проверяет D2 final source state и три protected APK. D3/source-baseline.zip:11735files,53477731bytes/SHA cb11d24818846cb3e427c88ba802abac1c69f94134cbeeb178668ff2ee988922. Source hashes включают все прежние изменения; не делать reset к HEAD28e6ff87203f0b93bf1935c9692b282d605b7072. Git status84lines/tracked diff58files сохранены до D3; Git глобальные настройки/индекс/коммиты не менялись.

В R разрешены лишь шесть новых .md этой сессии. Кодовые prototypes и tests находятся только D3/render, D3/taskqueue, D3/profiling; APK manifests inspected read-only в D3/android. Frozen stabilityaudit1 stage/его APK и D2 variants не изменены. В D3 tools лежит отдельный официальный host Perfetto v58.2 с проверенным SHA, не dependency приложения.

## Что выяснено

- GLES/ANGLE GL submission — один main SDL thread. Main FrameMove выполняет serial game scheduler/object updates/AI vision/Lua/default physics. Одна aggregate task последовательно исполняет seqParallel и seqFrameMT, перекрываясь с DoRender, join после Present. Wait может помогать чужим jobs; worker idle Event не busy spin. [Frame map][frame].
- Default mtPhysics выключен; mt_ai_vision config не делает vision async из-за false&&. Lua threads — coroutine общего VM. Global skeleton mutex, pathfinder scratch/restriction mutation, physics multi-pass и live object state не допускают bulk parallel rewrite. [Gameplay notes][game].
- D2 queue остаётся unsafe в R. Mutex reference не устанавливать автоматически; full policy fatal и host cost выше. Нужны publication/acquire, handle generation/lifetime, full/reuse и startup/shutdown membership. D3 уточнил старую zero-CPU underflow гипотезу: registration assertion раньше, underflow как normal constructor result не доказан. [Pool research][pool].
- CPU-001 camera sector comparator mismatch подтверждён. Нельзя исправить простой заменой Direction→Position: origin, load/reset, epsilon-crossing, invalid retry. Есть conservative **serial model** с exact position/geometry epoch; не integrated. Это первый небольшой кандидат после измерения стоимости. [Prototype/review][review].
- HOM/Detail и SSA missing dependency edges / global RNG shared state — новые strong source findings. Модель порядка не runtime race result и не C++ HB proof; relaxed completion/очередь всё равно требуют исправления. First reset frame may skip usual H/D joins: full quiescence не доказан. Не объявлять новой причиной crash. [Render notes][render], [review][review].
- All3 protected release APK не debuggable/no profileable. Shell callstack sampling на stock-device не обещать; system trace отдельно. Нет ADPF/Thermal integration; Java base31/native33/setThreads34/preferPowerEfficiency35. SDK34 Java не содержит35 методов. [APK/API preflight][apk].
- Предыдущие FHD data: render CPU и GPU оба существенны; нет full per-thread critical path/W/J и strict backend CPU delta. Thermal controls сняты cordonperf1; thermala1 D interrupted/excluded. [Historical index][history].

## Новые проверки, не повторять без новой гипотезы

- Sector gate:7scenarios+100000deterministicframes, stationary10000→1 query;4mutations; ASan/UBSan/leaks и standalone ARM64 API24 compile PASS. Сравнение с always-query synthetic static-topology reference; реальный CDB/картинка/время/энергия не проверены.
- Render dependency partial-order:3unordered conflicts,2missing-edge mutations; без TSan/actual scheduler/GL. Independent review готов.
- Actual ParallelFor headers+stand-in scheduler:44coverage/task-count cases, ASan/UBSan и ARM64 compile;31items/8logical workers→29tasks. Это модель гранулярности, не реальная engine TU.
- Event::Set:32finite x86WSL samples, no waiters, per-item overhead amortization only. Host→Android/energy перенос запрещён.
- ParallelForEach wait=false borrowed-function stack-use-after-scope воспроизведён ASan; known active callers wait=true. Не текущий gameplay crash diagnosis. Negative log сохранён.
- Perfetto host v58.2:3query correctness tests на synthetic rows + real builtin schema resolution PASS. Window clipping/UPID/incomplete/initial-frequency gaps проверены. Не настоящая device trace. Config template device parser ещё не проверялся.
- Actual three APK XML manifests/SDK javap проверены; контрольные hashes unchanged.
- Raw historical GPU inputs hashed, thermal aggregates recomputed from accepted repeat summaries. Новых аппаратных измерений нет; прежние queue stress/mutex/pacer/thermal tests не повторялись.

Скрипты и results: D3/render/run_sector_gate.py, check_render_dependencies.py, sector-gate-results.json; D3/taskqueue/new_hypotheses.py + results; D3/profiling/run_sql_tests.py + sql-test-results.json; D3/verify_historical_inputs.py; D3/android/apk_profiling_preflight.py. Их повторять только после scoped изменения/проверяемого вопроса.

## Следующее действие при наличии устройства

1. FIRST thermal restore_today.py --plan/--run по exactguards: config11673B/SHA b129ad651af57f7191734fe91dc98a80d9a1e85931e978a826bccbf94dd568b8 +28UIstates+12savehashes+stopped/screenOFF. В D3 restoration pending, никаких ADB не было. Не устанавливать APK доуспеха.
2. По [profiling plan][profile] снять доступный system trace/control observer-off: same save/camera/FBO1920×1080/quality/cap/backend warmups/thermal. Identify real TIDs/UPID, running/runnable/sleeping, CPU frequencies coverage, present cadence. Console закрыть ESC; stop/screenoff между блоками.
3. Missing app stacks — отдельный future profileable release variant после текущего запрета build/install; не подмена controls/debug APK. Никаких root/SELinux/affinity/frequency changes.
4. Измерить detect_sector count/ms и Skin4W counts/vCount; TaskWait CPU vs useful help/empty polls; GL driver/swap wait. Если camera query значим — отдельный serial epoch-aware gate с реальными CDB/lifecycle tests, не смешанный stability/render эксперимент.
5. Прежде чем расширять jobs: закрыть D2 scheduler и HOM/SSA/RNG/ownership prerequisites. Крупные AI/Lua/physics parallel kernels пока не оправданы. Worker-budget/ADPF только при measured CPU need и той же cadence/graphics.

Если устройства по-прежнему нет, безопасно продолжить **новый** bounded audit camera invalidation/CDB topology lifecycle или HOM input snapshots/Random consumers, без production implementation. Не менять queue/renderer и не перепроверять старые300k tests просто для активности.

## Готовый prompt следующему агенту

Продолжи D3 CPU/threading исследование собственного OpenXRay Android. Прочитай этот handoff, D3/session-state.json и пять связанных отчётов. Вся рабочая APK/код сохранены, новые prototypes только D3,6новыхMD в R. Не устанавливай mutex queue и не расширяй parallel AI/physics/Lua/GL: есть открытые D2 correctness и новые HOM/SSA/RNG/lifecycle hazards. Первый narrow experiment — измерение serial camera-sector query, затем epoch-aware gate если стоит. 100kmodel/44partition/32Event/Perfetto synthetic результаты не Android FPS/energy. Три release APK не profileable. При устройстве thermal restoreFIRST, потом controlled system profile; никакого ADB до разрешённого hardware stage. Сохрани все APK/userdata/results, не делай reset/clean и не смешивай экспериментальные направления.

[audit]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/OPENXRAY_THREADING_AUDIT.md
[frame]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/FRAME_EXECUTION_MAP.md
[ranking]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/THREADING_CANDIDATE_RANKING.md
[efficiency]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/ANDROID_CPU_EFFICIENCY_PLAN.md
[profile]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/ANDROID_CPU_PROFILING_PLAN.md
[game]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/gameplay/GAMEPLAY_THREADING_NOTES.md
[pool]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/taskqueue/TASKQUEUE_CPU_RESEARCH.md
[render]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/render/RENDER_THREADING_NOTES.md
[review]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/review-render-findings.md
[apk]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/android/APK_PROFILING_PREFLIGHT.md
[history]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/historical-evidence-index.json
