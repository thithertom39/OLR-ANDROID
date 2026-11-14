## D5 ПАУЗА пользователя,25 октября — читать первым

Планшет понадобился пользователю и уже отключён. Работа приостановлена; не ждатьADB/не продолжатьbuild/install/невыключатьПК. Точное актуальное состояние: W/work/stability-reload-20261025/PAUSED_STATE_20261025.md. Установлена stabilityaudit4 SHA35a5e80c1c738e2ad94cd518125fa2fd5e6b5620215ca0552716221ee3a57a53; обаbackend прошли10load/normalquit/settings+9same-processrestart иHOME/lock; новыйwatchdog реальноснималрегистры при12–28sloadбезстарогоfatal. Nativeallocatedheapрастёт~92MiB/restart,похожийростANGLE; источникнеустановлен. Общуюстабильностьнеобъявлять.

Готовтолькоstage диагностическойstabilitymem1(W/work/mem1-build11754files), НЕbuilt/installed; allnativepayloadmustexact4, меняетсятолькоprofileablemanifest/version/prebuiltpackaging. Profilerplanготов, реальнойtraceнет. Отдельныйnosoundcandidatehost/NDKPASSнеinstalled; архивныйкандидатнеbuilt. RproductionкодвсёещёНЕполучилдваXML/watchdogfixes, current4source=frozenstab4-build. FinaltestsettingsrestoreНЕзавершёнпередdisconnect: expectedANGLE/lastsavecg22ref. Пользовательможетсоздатьновыесохранения; наresumeпервымсвежийcapturebackup, неoverwriteстаройbaselineавтоматически. Все6APK/oldstagesсохранены, Gitreleaseпо-прежнемуотложен.

---

## D5: 25 октября — продолжается исправление DEV-004, читать первым

Текущая рабочая сессия: W/work/stability-reload-20261025/NEXT_RELOAD_SESSION.md и SA3_ACCEPTANCE_EVIDENCE.md. Установлена stabilityaudit3 SHA6bc910261f2fc943ecccab130cca42f117a1cbdb2400b0a4a62a0c3a3e1941da. Два внешних XML lifetime исправления прошли host/NDK/full APK и 20 загрузок/выходов/settings (10/backend), 18 same-process restart edges. ANGLE cg22ref/HOME/Recents/lock прошли. **Общая приёмка3 не пройдена:** свежий Native load cg22ref 04:11:48 упал внутри ft::on_probe → _Unwind_Backtrace → libunwind Dwarf step, вторичный SIGILL/xrDebug SIGTRAP. Готовится отдельная4 с безопасным interrupted-register snapshot, сохраняющим диагностику и stop/join. Ни4, ни архивный кандидат ещё не собраны/установлены.

R production пока НЕ получил два XML fixes или watchdog4; кандидаты в frozen stab2/stab3 и D5. Не собирать автоматически рабочий R как будто он равен установленной3. Все пять старых APK защищены. 25 октября10:44 свежий capture подтвердил12 исходных saves без изменений; config отличается только load_last_save после тестов; screenOFF/thermal0, connected. Restoration current tests pending перед новой установкой. Старое thermal restoration D4 завершено, не путать с этим. Не перезапускать старые finalizer, не сбрасывать Git. Подробности и точные пути — D5 checkpoint.

---

## D4: аппаратная проверка 24–25 октября 2026 — читать ПЕРЕД историческими checkpoint

Точный текущий результат: [DEVICE_ACCEPTANCE_RESULTS_20261024.md](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/DEVICE_ACCEPTANCE_RESULTS_20261024.md). Доказательства: W/work/device-acceptance-20261024-204608.

- Thermal restore ЗАВЕРШЁН: 28 UI, точный config и 12 saves; свежая финальная проверка D/final-device-verification/result.json 25 октября 00:38 MSK, PID отсутствует/экран Dozing. Историческое pending ниже больше не актуально. При следующем подключении сначала проверить свежие пользовательские файлы, не перезаписывать новые данные автоматически.
- Thermala1 настоящий native abort через ~12 s после нормального выхода: старый watchdog обращается к завершённому игровому потоку. D1 stop/wake/join проверен в ограниченных Native/ANGLE exit/settings случаях.
- Stabilityaudit1 повторный native main упал на устаревшем file_str XML-индекса. Отдельная stabilityaudit2 добавляет только обнуление file_str/tag_name; рабочий R и frozen stab-build не менялись. Новый frozen stage stab2-build, отдельные stability2-deps.
- Установлена stabilityaudit2, SHA d3acce465bcadb0d44cf7d83e36576dceb95eca71745a970173fb31f1e6f5c00. Первый Native load/exit/settings и повторное меню прошли, повторный load упал в XMLDocument::NavigateToNodeWithAttribute → CGameTask::Load. Это открытая DEV-003; полная приёмка и десять циклов/backend НЕ пройдены, ANGLE stabilityaudit2 не проверен. Не объединять кандидатов.
- D/xml-document-reuse-fix содержит следующий внешний кандидат ClearInternal: сброс m_root/m_pLocalRoot после DOM clear. Host cursor-contract test прошёл; actual NDK/full XML reload/APK/hardware ещё нет. Сначала проверить этот кандидат и собрать отдельную следующую версию. Stabilityaudit3 НЕ существует.
- Новые замеры thermala1 FHD: Native45.49/46.22, ANGLE43.87/42.21FPS, 2×20s каждый; это candidate-only наблюдения, не доказательство patch gain/экономии мощности или строгое сравнение backend.
- Старые APK/исходники/сейвы сохранены, новые журналы не публиковать автоматически. Старые D2/D3 finalizer не перезапускать поверх новых отчётов.

---

# Продолжение stability-сессии OpenXRay Android

## Актуальный checkpoint D2 — читать первым

23.10.2026, следующая offline-сессия после D1. **Рабочий код и три защищённых APK не менялись; все новые кандидаты находятся вне R. ADB/новой APK не было.** Новая основная сводка: STABILITY_FOLLOWUP_RESULTS.md. Состояние ниже раздела «Исторический D1 checkpoint» сохранено как история и не заменяет этот блок.

W = C:/Users/User/Documents/Codex/2026-10-11/gjvyb; R = W/work/openxray-build; D1 = W/work/stability-20261023; **D2 = W/work/stability-followup-20261023**. Python = C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe. Авторитетные final files: D2/session-state.json, source-delta-final.json, evidence-sha256.json. Перед новым изменением проверить hashes, а не сбрасывать Git.

### Что уже выполнено и не нужно повторять

- Точный checkpoint принят: D2/source-baseline.zip,11732 files,53453056 bytes, SHA dde82eec80a6c044bc1052877ff797b6b325242ec1b31382d576d03ddbf550de. Верифицированы19 изменённых ранее source files и7 D1 отчётов. Три protected APK сохранены. Директория W/work/stab-build и её frozen build inputs не менялись.
- **TaskQueue:** D2/taskqueue/TASKQUEUE_CANDIDATE_AUDIT.md и RESULTS_SUMMARY.json. queue-only, completion-acquire, allocator-guard, combined — отдельные исходные варианты, четыре настоящие NDK TU, детерминированные interleavings,300000 exactly-once tasks, по216 shutdown/restart sessions для queue-only/combined, ASan/UBSan и3 mutations. Baseline incomplete drain failure сохранён. Host mutex cost около2×enqueue/dequeue и12×empty poll — не внедрять автоматически. Fatal full/allocator guard не является graceful recovery. R queue остаётся прежней. Старый D1 sequence prototype не готов к интеграции, UBSan нашёл signed wrap subtraction около INTPTR_MAX.
- **ENG-005:** D2/decompression/DECOMPRESSION_CANDIDATE_AUDIT.md и INDEPENDENT_REVIEW.md. Финальный candidate v2: safe exact-size API, все3 consumer, decode-before-unload, archive range/cleanup/zero-stream guards. Старый rtc_decompress сохраняет capacity semantics.43 valid/1453 rejected/588 mutation inputs,7 реальных backup saves с прежним output,9 preview/10 load, по1152 ownership/100000 wide oracle cases FS_DEBUG on/off,6 mutations,4 реальных ARM64/API24 NDK TU. Exact patch application/reversal in memory. ASan default optimized profile и ASan/UBSan portable-copy host profile разделены, initial upstream alignment finding сохранён. Полной линковки/retail .db*/device не было. Всё вне R.
- **EGL:** EGL_CONTEXT_RECOVERY_PLAN.md + D2/egl/source-map.json (23 files) и recovery_model.py.19 policy tests Windows/WSL. Настоящее восстановление не реализовано: static VB/IB upload обычно выбрасывает host data, shader permutations/monolithic references и aliases требуют recipes/ownership. Model не вызывает GL/SDL и не доказывает recovery.
- **Launcher:** D2/launcher/LAUNCHER_ADDITIONAL_DIAGNOSTICS.md.200 rapid finish/100 late callback/400 AppLog close-resume; optional journal8 suites,800 concurrent records,40 acknowledged sync records retained при normal/forced Windows child exit, SDK34 compile и3 mutations. onDestroy trace сейчас entry-only; marker неоднозначен. L06 addition-only prototype не подключён, sync latency/export/Android runtime не проверены. Реальная причина пользовательского crash неизвестна.
- **Mods:** MOD_MANAGER_READINESS.md, D2/mod-foundation.49 methods (28 прежних byte-identical+21 новый) Windows/WSL,5 mutations;4 независимых MM patches against D1 prototype. Snapshot/generation, size+digest equivalence, directory payload policy, строгие persisted profiles и Unicode-name validation. Только host metadata/ZIP dry-run, без установщика/production integration.

Не запускать эти многократные тесты снова без нового изменения/гипотезы. Raw logs/preimages/failures сохранены. Проверять финальные hashes, не принимать review-v1 и review-v2-test-gap за final success.

### Что остаётся открытым

1. Production TaskQueue loss остаётся High: нужна full/reuse policy без silent loss и приемлемая ARM стоимость; completed Task* generation/ownership и worker membership/stats races ещё не решены. ASan/UBSan не TSan. D1 TSan environment failure не устранён.
2. Production rtc_decompress прежний. Подготовлен изолированный DEC candidate, но не применён. Внутренние game chunks/огромные allocations/archive tables/real CStreamReader map-remap/high-level null reader contracts — отдельные незакрытые границы. Decoder не обещает graceful handling любого испорченного игрового ресурса.
3. Full EGL loss остаётся missing capability; обычный background/surface recovery не равно context replacement. Сначала recipes/owner generation/barrier, потом отдельный renderer test harness. Нельзя glDelete старые names в новом context или вызвать обычный Device.Reset вслепую.
4. Launcher user crash не воспроизведён. Ни watchdog, ни GL, ни marker не названы установленной причиной. Новые host mocks не проверяют Android widgets/preferences disk IO/JNI teardown/ANR.
5. Доступная host foundation модов проверена, но безопасный importer/transaction/активация/ресурсная интеграция отсутствуют. Не добавлять Python runtime или параллельную VFS в игру. Сначала стабильность лаунчера и разрешение пользователя на интеграцию.

### Точный порядок следующего продолжения

Пока устройства нет — не вызывать ADB и не ждать его. Возможные новые offline работы: самостоятельный CStreamReader map/remap contract с ошибками mapping, аудит FS.r_open null consumers, Task* lifetime и bounded full policy. Сначала прочитать существующие результаты, не дублировать выполненное.

При возвращении планшета:

1. **FIRST thermal restore.** Прочитать NEXT_THERMAL_OPTIMIZATION_SESSION.md; W/work/thermal-power-20261023/restore_today.py --plan, затем --run по предусмотренным guards. Требовать restoration_passed, all_28_persisted_ui_states_equal_first_snapshot, cleanup_exact_config_verified, final_stopped_and_screen_off;12 original save hashes и config11673B/SHA b129ad651af57f7191734fe91dc98a80d9a1e85931e978a826bccbf94dd568b8. В D2 это НЕ выполнялось. Не устанавливать APK до успешного restore.
2. Сохранить текущие логи и user data. Сначала обычный пользовательский Exit→Launcher→Settings baseline capture без force-stop/duplicate MAIN. План D2/launcher/HARDWARE_CAPTURE_PLAN.md дополняет D1 план. Разделить Java exception/native signal/ANR/process death/navigation. Не стирать исходное evidence.
3. Для DEC нужен отдельный reviewed staging/variant, full link/export/new symbol и APK verifier перед установкой; подробности D2/decompression/HARDWARE_VALIDATION_PLAN.md. Frozen stabilityaudit1 не перестраивать/не перезаписывать. Не смешивать DEC, TaskQueue, EGL, mods, thermal A.
4. Queue-кандидат сначала review полноты/ownership/performance gates, затем свой отдельный hardware plan. Full/allocator diagnostic exit — failure candidate acceptance, не исчезнувший crash. Не отключать guards.
5. EGL реальная аппаратная проверка только после отдельного согласованного прототипа; сейчас его нет. Surface-only HOME/Recents/lock tests полезны, но не закрывают GL-005.
6. Все console-команды завершать ESC и проверять отсутствие консоли. Между блоками останавливать игру и гасить экран; settings возвращать. Не менять global Windows/Android settings, частоты или thermal policy.

Protected APK SHA256: cordonperf1=22f0ec05d00ee25ad9f62a4aafcbddc99d92eee98ca92e8f4a93d0d3e5738df2; thermala1=e54eccbedfa879cc3c9efedf4c492c31f50de84b8a759fd33c1f9f195c02b4dc; stabilityaudit1=adc06bb3dcf9dba378532f4b8ae07a182b360a3973514adb7999850238689fbf. Последний — прежний build PASS, **не installed/runtime accepted**. Последняя известная установленная версия thermala1; нового подтверждения сегодня нет.

### Краткая инструкция новому агенту

Продолжи собственный Android OpenXRay из D2/session-state.json и STABILITY_FOLLOWUP_RESULTS.md. Все D2 code candidates вне R, не объединять их в APK. Рабочий R содержит19 D1 fixes и прежние thermal A правки; frozen stabilityaudit1 stage исключает thermal A. Сохрани три APK/сейвы/settings. Прочитай независимые manifests/reviews/limits; не выдавай host proof за аппаратную приёмку. При устройстве first exact thermal restore, потом реальный ordinary Exit→Settings capture. Без устройства продолжай только новые проверяемые offline контракты. Проект менеджера модов host-only; backend Native GLES и ANGLE сохраняются. Git reset/clean, подмена frozen stage и автоматическая интеграция запрещены.

## Исторический D1 checkpoint

23 октября 2026. Планшет/контроллер недоступны по прямому указанию пользователя. **В этой задаче ADB не запускался. Пользовательский вылет лаунчера не воспроизведён и не объявлен устранённым.**

## Сначала прочитать

1. `PROJECT_STABILITY_AUDIT.md` — реестр severity/confidence, выполненное и открытые High.
2. `LAUNCHER_CRASH_INVESTIGATION.md` — сценарий выхода, Java/JNI/SDL, mock tests и аппаратный план.
3. `ENGINE_CORRECTNESS_AUDIT.md`, `GL_RESOURCE_LIFETIME_AUDIT.md` — конкретные native/GL дефекты и доказательства.
4. `STABILITY_FIXES_AND_TESTS.md`, D/concurrency/audit.md, D/filesystem/audit.md.
5. `MOD_MANAGER_ARCHITECTURE.md` — только после core stability; моды ещё не интегрированы.
6. `NEXT_THERMAL_OPTIMIZATION_SESSION.md` — обязательное незавершённое восстановление устройства.

W = `C:/Users/User/Documents/Codex/2026-10-11/gjvyb`; R = W/work/openxray-build; D = W/work/stability-20261023. Python = `C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`.

## Точный исходный checkpoint

До работы сохранены реальные **11 724 файла**, D/source-baseline.zip, SHA256 `28977feb0126f6654e056a2083274a916cdd63158cb6ed29de832cdef744b86b`. D/baseline-manifest.json хранит source hashes и protected APK hashes. Не делать reset к Git HEAD: база содержит предыдущие незакоммиченные исправления, ANGLE и две thermal A-правки.

Защищённые APK:

- W/outputs/openxray-soc-cordonperf1-arm64.apk — `22f0ec05d00ee25ad9f62a4aafcbddc99d92eee98ca92e8f4a93d0d3e5738df2`.
- W/outputs/openxray-soc-thermala1-arm64.apk — `e54eccbedfa879cc3c9efedf4c492c31f50de84b8a759fd33c1f9f195c02b4dc`.

Не перезаписывать их и не переустанавливать без причины. Последняя известная установка планшета — thermala1; свежего подтверждения в этой offline-сессии нет.

## Что изменено

Production source candidates (все аппаратно pending):

- Android: SDL clipboard lifetime; один uncaught-handler; корректные явные crash signatures; app-log append после Game destroy; `LifecycleTrace` со всеми шестью lifecycle callbacks обеих Activity.
- Native: bounded `-ltx/-fsltx`; минимальный save header/payload; null reader guards; preview buffer RAII; invalid load reader close; Lua scratch-buffer free и main-state unregister.
- GL: обнуление удалённых CRT/texture handles; `.seq` FPS validation; удаление OGM texture в error path.
- FS: POSIX fd sentinel/is_open с двумя callers; удаление всех archive entries; сохранность rename/index, self-rename, unindexed destination; bounded `-overlaypath`.
- Concurrency: watchdog stop/wake/join, signal/trace cleanup и restart; `FT_SHUTDOWN()` первым в `CApplication::~CApplication`; нормализация timed Event и обработка его ошибок.

Каждый патч/preimage/fixture находится в D/{launcher,engine,gl,filesystem,concurrency,root-fixes}. `x_ray.cpp` изменён дважды независимыми группами; не откатывать его целиком по раннему preimage поверх второго исправления.

Выводы ограничены source-extracted tests/NDK. Watchdog — Confirmed unsafe lifetime и Hypothesis как причина реального вылета. Clipboard — Confirmed observer leak и late calls, но его native callback не обязан падать. App log после выхода больше не теряет диагностику в tested mock orders, однако Android порядок нужно проверить на устройстве.

## Что не изменено и блокирует дальнейший релиз

- Production TaskQueue не заменялась. Детерминированный тест с capacity4096 доказывает потерю task при slot reuse. Отдельный sequence prototype — только D/concurrency, 80000 host tasks/4P4C; это не готовый scheduler для APK.
- `rtc_decompress` / общий `.db*` error contract не укреплён: открытая ENG-005 High. Existing decoder fixture выявляет дефект; не выдавать его за PASS патча. Изучить все три callers и safe decoder/failure propagation в отдельном кандидате до работы с чужими архивами.
- Полное восстановление после настоящей потери EGL context отсутствует. Обычный surface-preserving HOME/resume — другой сценарий.
- Unmanaged Lua coroutine keys, некоторые scheduler memory-order/shutdown paths, unchecked archive header bounds и неатомарные engine cfg/asset writes остаются по confidence в отчётах.
- TSan в текущем WSL падает с `unexpected memory mapping`; race-free заявлений нет.
- Ни FPS/тепло, ни аппаратная картинка/звук/AI/квесты/сейвы после новых патчей не проверены.

## Отдельная staging APK

Pipeline: D/pipeline/README.md и common.py/prepare_stage.py/build_stability.ps1/verify_stability_apk.py.

Stage: **W/work/stab-build**. Создан только после root review canonical plan SHA `276c01ed36c1e93edc7ee001d5d17ad05a39c6e1a8a79fdc5be0ed2417e73df2`. Frozen source manifest — D/pipeline/stabilityaudit1-inputs.json, **11 726 inputs** (включая local SDK properties). Source inputs исходного R, отдельные shared dependency hashes и overrides записаны.

Только в staging две thermal A-функции возвращены к pre-thermal cordonperf1 source из сохранённого thermal ZIP. Рабочий R не откатывался. Менеджер модов, queue prototype, новый pacer и GPU profiler не включаются. Изолированные dependency build dirs — W/work/stability-deps; исходники пяти прежних зависимостей берутся из W/work/deps без обновлений/downloads.

Команды из W:

```powershell
$taskPython = 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$taskPipeline = 'C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/stability-20261023/pipeline'
& $taskPython "$taskPipeline/prepare_stage.py" --check-stage
& "$taskPipeline/build_stability.ps1" -Run
& $taskPython "$taskPipeline/verify_stability_apk.py" --variant stabilityaudit1 --expect-angle present
```

**Полная сборка завершена: BUILD SUCCESSFUL за 9m13s, 47 tasks; APK verifier PASS.** Java compilation и lintVitalRelease прошли; полный lintRelease не запускался, compiler/deprecation warnings остаются. Обычная ARM64 APK: W/outputs/openxray-soc-stabilityaudit1-arm64.apk, **33 698 572 байта**, SHA256 `adc06bb3dcf9dba378532f4b8ae07a182b360a3973514adb7999850238689fbf`, `org.openxray.dev 1.6.02-stabilityaudit1`. Не установлена, runtime не проверен.

Проверены frozen inputs, прежний сертификат/v2, 29 ARM64 ELF/16K, ANGLE pair byte identity, shared C++ runtime/GLAD dispatch, пустой GitHub token, LifecycleTrace в DEX, сохранность старых APK/dependency sources и исключение двух thermal A-правок. Отчёт D/pipeline/stabilityaudit1-apk-report.json. Это проверка артефакта, не доказательство исправленного Android crash.

Последнее состояние читать в D/session-state.json и D/pipeline/build-*.log. Не путать промежуточный app-release.apk с проверенным экспортом. Не пересоздавать существующий stage и не обходить hash guards удалением каталогов. После изменения исходников нужен новый reviewed plan/variant, а не подмена frozen source.

## Первые действия при возвращении планшета

Не устанавливать новую APK сразу. Обязательный порядок:

1. Проверить фактическое подключение единожды, не polling без конца. Root — единственный оператор устройства.
2. Прочитать thermal handoff и провести **restore_today.py --run** из W/work/thermal-power-20261023. Он ещё ни разу не был успешно выполнен после disconnect. Сначала выполнить --plan/проверить original inputs.
3. Требовать новый restore result: `restoration_passed`, `all_28_persisted_ui_states_equal_first_snapshot`, `cleanup_exact_config_verified`, `final_stopped_and_screen_off` true; все12 original save hashes равны; user.ltx **11673 bytes / b129ad651af57f7191734fe91dc98a80d9a1e85931e978a826bccbf94dd568b8**. Исходные1728×1080/501/501/time10 возвращаются ради сохранности; это не изменение требований FHD для будущих performance tests.
4. Сохранить свежие APK/config/saves и passive exit evidence. Прежде чем менять APK, по возможности записать обычный пользовательский сценарий на контрольной сборке: штатный выход, настройки, полные logcat/Java/native/Activity данные. Не стирать исходные логи ради «чистого» файла; использовать отдельный capture и timestamps.
5. Только после restoration и проверки candidate report обновить APK с прежней подписью без uninstall/clear. Не обновлять живой игровой сеанс.
6. Выполнить D/launcher/plan_launcher_cycles.py: Native и ANGLE по10 normal-exit циклов, reversible setting change, повторный запуск, сохранность prefs/saves; отдельно HOME/Recents/lock. Нельзя имитировать Exit через force-stop или повторный MAIN.
7. Разделять AndroidRuntime exception, tombstone/native signal, ANR, process death и ошибку navigation. Session marker/lack of UI alone — не диагноз. Сопоставить PID/starttime/instance/task/sequence и штатный FT shutdown.
8. Между тестовыми блоками остановить игру и погасить экран. При thermal status≥LIGHT, сбое guard или нестабильности прекратить текущий тест, сохранить результаты, восстановить пользовательские настройки. Не менять global settings/thermal policy/частоты.

Offline план не делает ADB:

```powershell
& $taskPython 'work/stability-20261023/launcher/plan_launcher_cycles.py' --plan
```

## Менеджер модов: только reference foundation

D/mod-foundation содержит Python stdlib reference: `Mod`, strict manifest, pinned versions/dependencies/profile order, isolated save/config path plans, conflicts/provenance, immutable metadata resolver/cache, bounded ZIP inspection без распаковки. **28/28 tests Windows+WSL**, включая2000-path corpus. Не добавлен в APK/Gradle/engine и не открывает game bytes.

`MOD_MANAGER_ARCHITECTURE.md` основан на настоящем CLocatorAPI: last-registration wins, один winner без fallback, `.db*`/aliases/entrypoints, resource caches, Android регистр. Будущие слои расширяют существующий locator. Не включать вторую runtime VFS и не hot-reload ресурсы под живым engine. Перед production-интеграцией нужны аппаратная приёмка лаунчера и отдельное разрешение пользователя.

## Готовая инструкция следующему агенту

Продолжи stability-задачу из этого handoff и D/session-state.json. Сначала установи фактический результат staging build/verifier, не трогая старые APK/R/source freeze. Планшет отсутствует; не подключайся постоянно. При его возвращении FIRST thermal restore_today.py с exact original11673config+28UI+12saves, потом normal-exit reproduction и candidate tests. Не называй watchdog подтверждённой причиной пользовательского crash: подтверждён только дефект lifetime. Сохрани Native и ANGLE, графику/логику, разделение thermal/stability/mod prototypes. Не интегрируй mod reference и scheduler prototype. Открытые High — queue task loss, общий decoder/архивный error contract и полный context loss. Делай отдельные проверяемые кандидаты, фиксируй реальные ограничения и результаты, не заменяй устройство host tests.
