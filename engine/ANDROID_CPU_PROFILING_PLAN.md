# Galaxy Tab S9: воспроизводимое CPU/GPU/thermal профилирование

D3,23–24.10.2026. **Это подготовленный план; ADB, установка, запуск и аппаратные замеры в D3 не выполнялись.** Protected APK не менялись. System trace, app stack sampling и GPU diagnostics имеют разные требования; отсутствующий источник данных фиксируется как unavailable. [APK binary preflight][apk], [frame execution][frame].

## 0. Сохранность и обязательное восстановление

При будущем разрешённом подключении сначала [thermal handoff][thermal]: предусмотренные restore_today.py --plan, затем --run. Требовать исходный user.ltx11673B/SHA b129ad651af57f7191734fe91dc98a80d9a1e85931e978a826bccbf94dd568b8,28 persisted UI states,12 save hashes, restoration_passed и stopped+screenOFF. Новая APK до успешного восстановления запрещена. Сегодня restore не запускался.

Далее snapshot текущих APK/config/prefs/saves/logs, не overwrite originals. В production runtime не включать D2 queue/DEC/EGL/mod/journal вместе с CPU experiment. Контроллер не обязателен для system trace, но воспроизводимое действие/camera надо зафиксировать. Консоль закрывать **ESC**, визуально проверять. Между блоками stop game+screen off; при thermal≥LIGHT, instability/guard failure прекратить блок и сохранить evidence. Никаких global settings, root, frequency/affinity/thermal-policy writes. [Существующие guards][thermal].

## 1. Пре-флайт источников данных

| Канал | Что проверить | Чего не обещать |
|---|---|---|
| Perfetto system | Producer inventory/query, sched_switch/waking, process names, clock/trace bounds, dropped events, CPU frequency/idle | Наличие shell ADB не гарантирует vendor trace events или kernel stacks. |
| Native app call stacks | Фактический compiled manifest и device profiler eligibility; matching native Build IDs/symbols | Все3 текущих protected release APK не debuggable и без profileable. Суффикс .dev не разрешение на sampling. |
| App/ATrace scopes | Доступность app markers и реальные timestamps; нынешние Tracy macros в inspected TUs выключены | Наличие ZoneScoped в source не означает track в Perfetto. |
| GPU timings | Existing opt-in diagnostic variant/flag, EXT_disjoint_timer_query support, counter bits/availability/disjoint/drop stats | GPU wall/CPU gl-call duration не GPU execution; vendor GPU counters могут быть закрыты. |
| Present cadence | Собственный SDL SurfaceView layer/PID, actual present timestamps, continuity/ring wrap | App FrameTimeline может отсутствовать для SurfaceView; не подменять его UI/launcher frames. |
| Thermal/power | thermalservice/API support, vendor units/override; PowerStats/rails availability, charging context | AP raw не Celsius, battery current на зарядке не полная power, unavailable не0. |

Официальные основания: [CPU scheduling](https://perfetto.dev/docs/data-sources/cpu-scheduling), [frequency/idle](https://perfetto.dev/docs/data-sources/cpu-freq), [callstack eligibility](https://perfetto.dev/docs/getting-started/cpu-profiling), [profileable release](https://developer.android.com/guide/topics/manifest/profileable-element), [FrameTimeline limitations](https://perfetto.dev/docs/data-sources/frametimeline). Actual manifest/SDK facts уже проверены [локально][apk].

На текущем release начать с system scheduler trace и доступной существующей buffered telemetry. Если отсутствие stack attribution блокирует вывод, следующим отдельным этапом нужен profileable **release** variant с теми же optimization flags/symbol IDs; он не создавался здесь. Debug APK не равная baseline для timing. Нельзя обходить ограничения root/SELinux changes. Native sampling начать с умеренной частоты, например100Hz, коротким отдельным окном и observer-off control; частоту не выдавать за гарантию разрешения всех коротких функций. [Perfetto sampling guide](https://perfetto.dev/docs/getting-started/cpu-profiling).

## 2. Подготовленные файлы

В D3/profiling:

- [system-trace.pbtxt][config] — baseline30s/32MiB ring-buffer template: scheduler lifetime/wakeup, CPU frequency/idle, gfx, process names, initial/periodic cpufreq polling. Только схема будущего capture, не выполненный device config. Producer/event availability и принятие device parser ещё не проверены.
- [thread_cpu.sql.in][cpuquery], [thread_states.sql.in][statequery], [frequency_residency.sql.in][freqquery] — queries с обязательными start/end ns и lifetime-specific UPID. Нет hardcoded живого PID.
- [sql-test-results.json][sqlproof] — все3 query expressions проверены в **настоящем Perfetto Trace Processor v58.2**, но на synthetic tables; также разрешаются реальные built-in schema names на empty trace. Не Android trace/benchmark.
- tools/trace_processor_shell.exe — официальный pinned host analyzer, скачан только внутрь D3,14439936B/SHA adfa6bad3d72be3ba9b83fa2b17b69fa13b3ab1cad0f42e52b86188bd5f0f997. Version v58.2-add693d8b, full commit add693d8b338ba9599dbcbc3e300b1ab8c000897. Проверен hash из [официального manifest](https://get.perfetto.dev/trace_processor); [local provenance][tool]. Не installer и не dependency приложения.

Шаблоны SQL заменяют __START_NS__/__END_NS__/__UPID__ только на проверенные integers; table placeholders для реальной трассы — sched_slice/thread/thread_state/counter/cpu_counter_track. Сначала выбрать UPID через process inventory и нужную lifetime/PID, затем одно общее foreground window после прогрева. Queries не суммируют threads разных процессов с одинаковым PID/name. [Реальный host test][sqlproof].

SQL корректно отрезает slices на границах, исключает dur<0, не заполняет неизвестную начальную frequency нулём, считает time-weighted frequency только по известному покрытию. Синтетический oracle проверяет main70ms + worker20ms в100ms window и80% frequency coverage на одном CPU. Это **арифметические тестовые значения**, не результаты планшета. Отсутствующие/incomplete ranges требуют отдельного coverage/trace-loss отчёта, а не экстраполяции.

Будущий capture можно выполнить штатным shell Perfetto с text config и отдельным output filename на устройстве, затем pull как binary. Точный пример для операторской сессии после всех guards:

```text
adb shell perfetto --query
adb shell atrace --list_categories
adb push system-trace.pbtxt /data/local/tmp/oxr_cpu_<session>.pbtxt
adb shell perfetto --txt -c /data/local/tmp/oxr_cpu_<session>.pbtxt -o /data/misc/perfetto-traces/oxr_cpu_<session>.pftrace
adb pull /data/misc/perfetto-traces/oxr_cpu_<session>.pftrace <new-PC-evidence-folder>
```

Это инструкции, **не выполненные команды**. Под Windows при необходимости использовать полный путь/ .\adb. Не перенаправлять binary trace через text PowerShell redirection; pull сохраняет байты. Не запускать killall/очистку чужих traces. Если producer/config не поддерживается, сохранить ошибку и сузить config, не трактовать empty trace как0CPU.

## 3. Идентификация и scopes

Снять PID/starttime/UPID и все TID/name/priority: native SDL main, Java UI, Task Workers, OpenAL/OpenSL mixer, audio prefill исполнители, logcat capture, watchdog, ANGLE-GC/driver workers, framework service threads. Сопоставить роли по stacks/markers, не по одному имени. Выяснить реальный core topology/capacity/cpufreq policies из read-only доступных данных; frequency не назначает автоматически big/little. [Source topology][audit], [APK preflight][apk].

Для будущей отдельной lightweight instrumentation нужны:

| Scope/counter | Зачем |
|---|---|
| Frame start/end, update, aggregate task | Frame ID/session/monotonic anchor и overlap, complete main/worker accounting. |
| Object/scheduler/AI/ALife/Lua/physics | Body CPU, requested/executed physics substeps, callback counts/age, individual long Lua resume. Не менять budgets. |
| Bones/Skin4W | Actual path, vCount histogram, lock wait vs body, task split/dispatch/merge и результат. |
| HOM/detail cache vs visibility/main/sun/light prep | Producer dependencies, jobs' input epoch, wait targets, actual useful CPU. |
| TaskWait | Useful executed-task CPU, empty-help loops, completion target/generation, parked/spinning split. |
| Submission/upload/driver and swap | Count/state/uniform calls, streamed bytes/map stalls, shader/pipeline cold misses; не per-draw heavy logging. |
| Limiter | Selected cap/effective args, sleep request/actual elapsed, present cadence и display vote. |

Current device.cpp android_perf имеет elapsed FrameMove/SceneRender/Present/TaskWait/LimiterSleep и thread CPU summary; GPU profiler покрывает render scope, не все simulation/worker time. Future scope placement должна учитывать helper execution и nested inclusive regions; не складывать parent+child. Ring buffering/sample rates, flush вне measured window, no per-task disk IO. First measure observer overhead отдельно. Это **план**, source instrumentation не добавлено. [Frame map][frame], [render profiler sources][render].

## 4. Сцены и равные условия

| Сцена | Фиксация | Что выясняет |
|---|---|---|
| G1: тяжёлый Кордон у моста/раненого | Копия известного сейва, зафиксированная камера, FBO1920×1080/render_scale1, одинаковые AA/grass/shadows/lights | Graphics-heavy reference; уже есть raw historical data, новые timestamps должны быть синхронизированы. |
| C1: NPC/анимация у лагеря/АТП | Один pristine save, одинаковое число/состояние NPC, камера, время/погода, emitters | Bones/AI/Lua/audio cost; это CPU-heavy **кандидат**, подтверждается trace, не названием сцены. |
| C2: разбитие ящика/физика | Каждый repeat заново pristine save, одинаковое действие и интервал после него; count fragments/contacts/substeps | Persistent compute vs transient loading/stall; не сравнивать новое разрушение с уже успокоенной физикой. |
| L1: controlled resource load | Один маршрут/save/cache state, отдельно first-use и warmed repeats | FS/decode/shader compilation; cold/warm не смешивать, системные caches не очищать. |
| S1: camera sky/control | Отдельное направление и отдельная маркировка | Пониженная GPU scene complexity для диагностики; **не эквивалентная картинка G1** и не оптимизация. |

Для пары фиксировать APK hash/Build IDs/backend GL_RENDERER, own save hash и camera vector, world clock+weather, FBO/drawable/display refresh, effects/AA, cap и unlock flags. Не менять resolution/плотность/тени/сглаживание ради gain. При временной заморозке environment time различать time_factor_single/al_time_factor и Device TimerGlobal; сохранить/вернуть exact config и проверить реальные queried values. Старое сравнение с разным солнцем уже признано недействительным. [Cordon history][history], [thermal session][thermal].

## 5. Порядок серий

1. Сначала обычное приложение **без тяжёлой диагностики**: warmed stable scene,30s present window, thermal readiness. Сохранить launch/load screenshots и закрытую console.
2. Отдельный system scheduler window30s в той же сцене, затем повтор observer-off; оценить trace/collector overhead. Старый thermal collector --thermal-only5s guard допустим только с прежним readiness overhead gate; full inventory/proc snapshots вне FPS window.
3. При необходимости отдельно app stack sampling и отдельно sampled GPU timestamps; не включать всё одновременно без сравнения observer effect.
4. Native/ANGLE в чередующемся порядке, затем обратный порядок после охлаждения; желательно≥3 accepted repeats каждого режима. Одинаковый cap, warmup policy, зарядный режим, brightness/display policy и начальный thermal state. Не устанавливать новый режим поверх живого игрового сеанса.
5. Sustained test отдельными controlled блоками: отмечать возраст с запуска/нагрев/thermal flags, stop/cool между блоками. Нельзя сравнивать холодное начало одного backend с прогретым концом другого. Исходные thermal guards сохраняются; не ждать перегрева ради graph.
6. Только после baseline — ОДИН отдельный кандидат/переключатель за серию. Current research не содержит установленного кандидата; любые будущие installs — отдельный этап.

## 6. Выходные метрики и трактовка

- Per-thread running ms/window и ms/presented-frame **только при совпадающем точном окне**, runnable delay, sleep/blocked states, CPU/core residency, migrations. Суммарный CPU>wall допустим для нескольких cores, но не равен percent всего SoC без явного denominator. [Scheduling source](https://perfetto.dev/docs/data-sources/cpu-scheduling), [SQL][cpuquery].
- Main critical path: elapsed/CPU по stages, TaskWait полезные jobs vs spin, lock/futex wait с attribution. Sleeping S, uninterruptible D и runnable R не являются сами по себе названиями причины; stacks и scopes обязательны. [State query][statequery], [frame map][frame].
- Frequency residency с coverage и unavailable flags; initial gap неизвестен, не0. Не выводить core capacity/энергию из единственной частоты. [Frequency guide](https://perfetto.dev/docs/data-sources/cpu-freq), [query][freqquery].
- Presented FPS = intervals/sum(duration), P50/P95/P99 nearest-rank,1%low через mean худших ceil(1%N), не обратный P99. Разрывы history/слоя/PID/refresh changes раздельно; GPU and CPU clocks синхронизировать explicit anchors, иначе только window-level summaries. [Historical parser contract][history].
- GPU sampled stage elapsed с availability/disjoint/epoch и counts; draw/detail counters — submissions, не unique objects. Нужны одинаковые эффекты; render stage scopes inclusive. [Render audit][render].
- Thermal status/headroom/charging и raw sensor provenance. Power rails если реально доступны: интегрировать energy по тому же window и делить на presented frames; battery counters — whole-device/charging-sensitive. USB current не принять за SoC watts. Нет rails/external reliable power — **нет количественного power claim**. [Power counters](https://perfetto.dev/docs/data-sources/battery-counters), [existing thermal limitations][thermal].

В отчёте: source/candidate hashes, scenario/settings, raw trace/log paths, timestamps/window/UPID, repeated samples/spread, missing data и observer comparison. Gain внутри разброса не подтверждён. Если same-quality frame GPU-bound и TaskWait пустого CPU почти нет, серьёзное распараллеливание не обосновано: приоритет driver/state/bandwidth/pass work, показанные профилем, без изменения картинки. [Decision ranking][ranking].

[apk]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/android/APK_PROFILING_PREFLIGHT.md
[frame]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/FRAME_EXECUTION_MAP.md
[thermal]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/NEXT_THERMAL_OPTIMIZATION_SESSION.md
[audit]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/OPENXRAY_THREADING_AUDIT.md
[history]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/historical-evidence-index.json
[render]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/render/RENDER_THREADING_NOTES.md
[ranking]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/THREADING_CANDIDATE_RANKING.md
[config]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/profiling/system-trace.pbtxt
[cpuquery]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/profiling/thread_cpu.sql.in
[statequery]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/profiling/thread_states.sql.in
[freqquery]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/profiling/frequency_residency.sql.in
[sqlproof]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/profiling/sql-test-results.json
[tool]: C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/threading-cpu-20261023/profiling/tools/provenance.json
