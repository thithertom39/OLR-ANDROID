# Продолжение thermal / power оптимизации OpenXRay Android

Дата состояния: 23 октября 2026 года, после отключения планшета около 15:47 MSK.

**Обычная thermala1 уже установлена. Native FHD load показал правильный мир без консоли, но ни одного candidate SF-окна не снято: следующая measure-попытка оборвалась на device not found ещё до начала capture. Candidate ANGLE не запускался. Восстановление пользовательских настроек остаётся pending. Первое действие при переподключении — выполнить restore_today.py, а не продолжать замеры.**

Уменьшение мощности, энергии на кадр или температуры двумя A-правками не доказано. 60 FPS в тяжёлой контрольной сцене не достигнуты. После потери соединения нельзя подтвердить текущее состояние PID/экрана или считать cleanup успешным. Новых ADB-команд при подготовке этого handoff не выполнялось.

## Пути, чтение и владелец устройства

- W: `C:/Users/User/Documents/Codex/2026-10-11/gjvyb`.
- R: `W/work/openxray-build`.
- D: `W/work/thermal-power-20261023`.
- OLD: `W/work/cordon-gpu-20261022`.
- Python: `C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`.
- ADB: `W/work/android-sdk/platform-tools/adb.exe`.
- Serial `R52X40ES4LE`, устройство SM-X710 / Galaxy Tab S9, приложение `org.openxray.dev`, последний проверенный UID **10361**.
- Game root: `/sdcard/Download/socc/sas/soc`; данные: `.../_appdata_`; log: `.../_appdata_/logs/openxray_u0_a361.log`.

Сначала прочитать [ANDROID_THERMAL_POWER_AUDIT.md](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/ANDROID_THERMAL_POWER_AUDIT.md), [THERMAL_DEVICE_VALIDATION_PLAN.md](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/THERMAL_DEVICE_VALIDATION_PLAN.md), этот handoff и D/{cpu-lifecycle,pacing,gpu-memory}/audit.md. Подробности предыдущих защищённых исправлений — R/CORDON_GPU_PERFORMANCE_REPORT.md и R/NEXT_GPU_OPTIMIZATION_SESSION.md. Вчерашние +12.06% grass batching — отдельный render-performance результат, не доказательство текущего энергосбережения.

Root — единственный оператор ADB. Подагенты работают офлайн и не управляют устройством. При возобновлении нужен свежий PID; старый PID 21537 из Native launch не переносить в новый collector.

Root обновил D/session-state.json и D/root-verification.json: candidate установлен, C game-after подтверждён около 15:43, устройство отсутствует, восстановление ожидает reconnect. Первичные доказательства установки и запуска — соответствующие result.json. Свежая офлайн-сводка D/analysis-device-final.{md,json} учитывает C visual proof и candidate abort: **0 строгих сравнительных пар**, W/J неизвестны. Более ранние analysisresults-* и старые checkpoint-записи не заменяют эту сводку.

## Сначала восстановление сегодняшнего состояния

Исходные данные сегодняшнего пользователя сохранены в **D/device-baseline-132003**, включая manifest и все 12 файлов savedgames. Оригинальный user.ltx:

- **11 673 байта**.
- SHA256 `b129ad651af57f7191734fe91dc98a80d9a1e85931e978a826bccbf94dd568b8`.
- `vid_mode 1728x1080 (0Hz)`, AA off, VSync off, `rs_refresh_60hz off`.
- Gameplay/menu limits **501/501**.
- `time_factor_single 10.`.
- Launcher: **Native OpenGL ES**, **1080p Screen (1728x1080)**, custom args пусто, **unlock=false**.

Все **28** исходных UI-состояний находятся в `D/device-runs/control-native-uncapped-134650-723938/launcher/settings-before.json`. Не подменять сегодняшний original вчерашним 11 703-байтным user.ltx, вчерашним ANGLE selection или старым набором четырёх quicksave. Сегодня проверяются все **12** сохранённых файлов, включая owned cg22*; wildcard-удаление extras не делать.

Последняя известная временная конфигурация после candidate D launch — **FHD 1920×1080, Native, config 60, unlock=false, time_factor_single/al_time_factor 0.001**. Она могла остаться на планшете после отключения. `user_ltx_unchanged=true` при install означает лишь, что install-r не изменил тогдашний файл; это **не** подтверждение восстановления исходного времени/разрешения.

[restore_today.py](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/restore_today.py) прочитан и проверен root; офлайн он не запускал ADB. Default/--plan только проверяет локальные inputs. --run предназначен для root после реального reconnect:

```powershell
Set-Location 'C:/Users/User/Documents/Codex/2026-10-11/gjvyb'
$taskPython = 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'

& $taskPython 'work/thermal-power-20261023/restore_today.py' --plan
& $taskPython 'work/thermal-power-20261023/restore_today.py' --run
```

Не выполнять --run сейчас при отсутствующем устройстве. После reconnect это действие должно предшествовать новой FPS-серии. Helper:

1. Проверяет локальный config hash и 12 исходных save hashes; останавливает приложение и возвращает точный сегодняшний user.ltx.
2. Сравнивает все 28 UI-состояний; меняет только разрешённые четыре controls — backend, custom args, unlock, resolution. При отличии остальных 24 прекращает работу, не исправляя их вслепую.
3. Нажимает **Start Game**, поскольку launcher сохраняет preferences только в launchGame/savePreferences. Простая смена controls с последующим force-stop не обеспечит persistence. Этот короткий запуск не загружает save.
4. Останавливает игру, снова возвращает точный user.ltx, заново открывает launcher и проверяет persistence всех 28 состояний.
5. Проверяет обратным pull все 12 saved files, завершает package stopped/screen OFF и сохраняет новый `restore-today-*/result.json`.

Восстановление завершено только при всех `restoration_passed=true`, `all_28_persisted_ui_states_equal_first_snapshot=true`, `cleanup_exact_config_verified=true`, `final_stopped_and_screen_off=true` и совпавших 12 hashes. Пока такого успешного результата нет. При ошибке читать новый result/evidence; не объявлять восстановление по наличию push. Проверка private SharedPreferences ограничена 28 UI-состояниями после рестарта; побайтового private XML backup без root не заявляется.

Helper не откатывает APK и не меняет saves/global settings. **Сначала восстановить пользовательские 1728/501/обычное время и UI. Только затем, для отдельно разрешённого продолжения D, снова создать тестовую FHD/.001/config 60 конфигурацию через device_session.py.** По окончании нового тестового блока выполнить точное восстановление снова.

## Что сохранено и что действительно установлено

До двух A-правок сохранено фактическое рабочее дерево: D/source-baseline.zip, **11 720 файлов, 53 333 478 байт ZIP**, SHA256:

`d4f8f1a581c8dbb8a91aacbbce4a12cad6925db6c7db21098164bcedd8e6194b`.

Хэши файлов — D/baseline-manifest.json. Итоговая сверка D/source-delta-final.json: все 11 720 baseline-файлов присутствуют; изменены ровно два ожидаемых production-файла, перечисленных ниже. Новые отчёты/helpers/fixtures учитываются отдельно. Снимок содержит прошлые незакоммиченные исправления и нужные JNI libraries, без ключей и build caches. **Не сбрасывать Git на HEAD**; reset/commit/push/release packaging не поручены.

| Артефакт | Состояние | SHA256 |
|---|---|---|
| W/outputs/openxray-soc-cordonperf1-arm64.apk | Старый неизменный контроль, сохранён отдельно | `22f0ec05d00ee25ad9f62a4aafcbddc99d92eee98ca92e8f4a93d0d3e5738df2` |
| W/outputs/openxray-soc-thermala1-arm64.apk | **Установлен**, normal version 1.6.02-thermala1, 33 697 516 байт | `e54eccbedfa879cc3c9efedf4c492c31f50de84b8a759fd33c1f9f195c02b4dc` |

D/install-thermala1-154431/result.json подтверждает установленный APK hash, **UID 10361**, неизменный при install user.ltx и **12 save files unchanged**. Install выполнен обновлением, без удаления приложения/данных; private prefs сохранены штатным install-r, не private XML hash. Более ранняя D/install-thermala1-154332 не является успешной установкой.

Normal APK build/verifier PASS: 29 ARM64 libraries, 16K ELF compatibility, прежние сертификат и C++ runtime, ANGLE присутствует, direct graphics imports/protected fixes проверены, GPU profiler OFF. Evidence: `outputs/angle-work/thermala1-apk-report.json`, `outputs/angle-work/thermala1-inputs.json`, `D/thermala1-build.log`.

**Не запускать install_candidate.py повторно автоматически.** Он рассчитан на исходный установленный cordonperf1 и проверяет старый hash перед install. Сейчас thermala1 уже установлен; повторная установка не нужна для продолжения D. Сначала readback/проверка текущего APK после reconnect; не делать uninstall или reset data.

## Два A-изменения и их доказательства

| Изменение production | Точный объём | After source SHA256 |
|---|---|---|
| src/xrSound/SoundRender_CoreA.cpp::update_listener | Удалён только повторный `AL_VELOCITY=0`; zero initializer, position/orientation/effects сохранены | `c88fb2dcffffe551e53f71d7eb4ed1bf18e4b62c1290fd0ac02068fbdfb56355` |
| src/Layers/xrRender_R2/r3_rendertarget_accum_point.cpp::accum_point | Удалены только unused m_Texgen_J и его вычисление; screen texgen/uniforms/draws сохранены | `52a5713cddc70235e76b9e035a60321011b00b8ee2d0895e58baf40939d560c3` |

Listener: 49 152 source-extracted differential updates / 12 contexts, две mutation-проверки, WSL ASan/UBSan+leaks и actual ARM64 NDK27 TU PASS. Реальные исходные/изменённые функции дают одинаковый final listener state и остальные вызовы; modeled listener calls 147456→98304. D/cpu-lifecycle/listener-implementation.json и listener-a1.patch.

Point light: 8192 source-extracted cases, render/state/uniform bytes равны, две mutations, ASan/UBSan, NDK diagnostics OFF/ON PASS. Два dead matrix multiply и identity убраны на вызов; число lights/frame и цена этой работы не измерены. D/gpu-memory/A1-host-results.json, A1-change.json, A1-unused-jitter.patch, ndk-results.json.

**Actual APK binary proof PASS**, D/cpu-lifecycle/final-binary-proof/{README.md,proof.json,manifest.json,function-manifest.json}:

- Exact listener export 0x4d198 size 196→176: убран именно пятиинструкционный zero-velocity/AL_VELOCITY block. Остальная функция равна после нормализации PLT/local branches. Position/orientation и initializer BL0x4ccd0 сохранены; exact function подтверждён vtable relocation slot+0xb8.
- Exact accum_point export 0x3400a0: прямые Fmatrix::mul BL 3→1, identity 1→0. Проверены arguments/PLT/GOT/JUMP_SLOT, не глобальное число символов. Standalone jitter helper остаётся для других callers. Function size 13440→13468 из-за codegen не выдаётся за выигрыш.
- Candidate Build IDs: libxrSound `d208c0e65d297085bfe87f02e7da75401c68704a`; libxrRender_GL `8eacc36379ddac95e07fad0107606d976e106f3c`. libopenal побайтово одинаков в обеих APK.

Production-картинка, inputs, AI/physics cadence этими локальными правками не менялись. Host/бинарные проверки не заменяют device audio, lighting, resume и sustained acceptance; milliseconds/allocations/energy gain не установлены.

## Аппаратные данные, пригодные для описания

Все A/B/C ниже — **старый cordonperf1**, не candidate. FBO 1920×1080, drawable 2560×1600, owned cg22benchp4, AA off, VSync off, menu 60, оба queried clock factors 0.001. Для каждой группы два примерно 30-секундных SF-окна. Root визуально подтвердил правильный мир без консоли A/B и **C game-after около 15:43**. C-after уже не pending по последнему факту root.

Камера: `[-55.414333,-2.253043,-64.284225,0.928497,-0.231284,0.290519]`. Это предоставленные root scene coordinates, не per-frame camera trace. Одинакового SAV и факторов .001 недостаточно для самостоятельного доказательства одинаковой абсолютной погоды/часов; weather_clock_id в verification не следует выдумывать.

| Группа | Launch evidence | Measure evidence | FPS r1/r2; weighted |
|---|---|---|---|
| A, Native uncapped, config 501 + unlock | control-native-a-fhd-145940-029638 | control-native-a-fhd-150518-617589 | 46.082 /46.023; **46.053** |
| B, Native existing config 60, no unlock | control-native-b-fhd-151046-274492 | control-native-b-fhd-151738-316785 | 46.126 /45.786; **45.956** |
| C, ANGLE existing config 60, no unlock | control-angle-c-fhd-152241-739317 | control-angle-c-fhd-152952-066113 | 41.718 /39.734; **40.726** |

Все указанные папки относительно D/device-runs. Точные SF folders находятся в sf-run0.stdout/sf-run1.stdout; сами capture/latency/present-analysis файлы сохранены в OLD/captures. Median во всех шести окнах около 25ms; P99 около 33.333ms, 1% low Native около 30FPS, ANGLE 28.364/25.714. P95 A1≈33.333/A2≈25.000ms, B/C≈33.333ms. Интервалов>100ms в этих коротких окнах нет; это не долгосрочная гарантия отсутствия фризов.

Readiness thermal-only: A 125 ms, B 109 ms, C 93 ms; максимальные samples 125/141/140 ms. Guard status 0, override false, USB powered true. AP raw initial→последний полностью внутри SF: A 27.7→48.0, B 28.4→48.0, C 35.4→47.8. C terminal AP 50.1 уже после SF не подставлять как in-window endpoint. Vendor units unknown; не превращать эти числа в измерение W/J или физический thermal выигрыш.

Full context снимки вынесены за SF. Whole-process ticks при CLK_TCK 100 дают A 86.62/B 87.44/C 89.67 CPU seconds между внешними endpoint snapshots, включая все threads и время вне точных SF-границ. Это не render CPU time, utilization или энергия. Частоты — raw endpoint snapshots, не average/fixed frequencies.

Native тяжёлая сцена остаётся около **46FPS**, а не 60: config 60 здесь не показал полезного idle времени или снижения тепла. C стартует заметно теплее и использует другой API; не выдавать A/B/C за строгий patch comparison либо ранжирование энергоэффективности backends.

### Candidate: ровно что выполнено и чего нет

- **Установка подтверждена**: D/install-thermala1-154431/result.json.
- **Native D load подтверждён**: D/device-runs/candidate-native-d-fhd-154511-255525/result.json; queried clocks .001, actual_fbo_size[1920,1080], root просмотрел loaded-before-esc.png: правильный мир, консоли нет. Вложенный launch.stage описывает меню до последующего load, а не итог отсутствия мира.
- **Candidate SF отсутствует**: D/device-runs/candidate-native-d-fhd-154725-677461/result.json — ошибка `adb.exe: device 'R52X40ES4LE' not found` уже на thermalservice preflight, до PID/readiness/SF. Это **не нулевой FPS**, не crash и не незавершённое валидное окно.
- Candidate Native uncapped E не снят. Candidate ANGLE F **не запускался**. Полная runtime acceptance, sustained test и restoration не завершены.

### Обязательные исключения

- `control-native-uncapped-141937-672886`: VALIDITY_NOTICE.json, full proc/sysfs observer занимал 9.3–10s/sample и сам создавал нагрузку. Не baseline ни FPS, ни тепла.
- `control-native-uncapped-valid-144047-753568`: readiness 953 ms, SF не запущен, FPS отсутствует.
- `control-native-a-144957-847914`: launch фактически 1728×1080, без SF; не объявлять FHD. Причина: applied_gfx_signature уже считался FHD, а повторный push исходного 1728 user.ltx обходил повторную настройку launcher. Актуальный device_session.py явно пишет vid_mode1920 в test copy и проверяет **actual** engine FBO. Не полагаться на подпись UI/scene_note.
- `install-thermala1-154332`: не успешная установка; использовать 154431.

## Collector, анализатор и точность выводов

D/telemetry/power_samples.py по умолчанию --plan, без ADB. Текущие **36 host tests PASS** — D/telemetry/host-tests.log; старый offline-proof25 относится к раннему набору проверок и не является текущим счётчиком. Collector не делает install/input/stop/settings/reset; он сообщает stop decision, а устройством управляет root.

Для SF используется **--thermal-only**: только thermalservice + pidof раз в 5 s, не battery/proc/sysfs/threads. First flushed readiness должен быть ready=true и measured overhead<=200ms до запуска SF. Full inventory один раз вне FPS; после этого readable-paths и full context snapshots только до/после окон. --threads opt-in, не включать по умолчанию. Любое завершение sampler, пока SF child жив, прерывает capture. Exit2 по intentional force-stop **после** sf_capture_complete допустим только при соответствующем cleanup marker/timestamps; thermal stop или PID loss внутри SF так не оправдывается.

D/analyze_device_runs.py: **13 self-test scenarios PASS**; реальный default CLI проверен с traps на filesystem writes/subprocess. Default stdout/read-only; --write --out создаёт свежие MD/JSON, старые файлы не перезаписывает. Читает actual engine FBO, SF result из stdout→capture folder, readiness/guard/context, config/cmdline/renderer/time factors. Raw missing/denied остаётся missing; W/J=null. Не усредняет P95/P99/1% low из summaries. Weighted FPS = sum(intervals)/sum(duration).

Для строгих пар требуется одинаковая policy/backend/FBO/config, сопоставимый initial AP raw и отдельные root proofs console/camera/scene/weather/installedAPK. D/root-verification.json содержит актуальные аттестации A/B/C и Native D load; C-after уже подтверждён root. Анализатор может честно оставить все strict pairs ineligible из-за отсутствующего weather/absolute-clock proof. Это не повод заполнять доказательства догадкой.

PowerStats-попытка сохранена в D/powerstats-inventory.json: framework service существует, default dumpsys вернул 0bytes; `android.hardware.power.stats.IPowerStats/default` не найден. W/J не получены. USB battery current не равен полной мощности устройства, temperature — инерционный outcome. Не выводить энергию из FPS, частоты, sleep, AP raw или status 0.

## Разрешённое продолжение после restoration-first

1. Root после reconnect выполняет restore_today.py и проверяет полный успешный result, затем сохраняет свежий readback установленного APK/config/UI/saves. Пока устройство отсутствует — только офлайн анализ/сборка; не запускать циклические ADB-попытки и не объявлять cleanup.
2. Если пользовательское поручение продолжать hardware tests остаётся в силе, отдельно начать новый candidate D Native existing60 блок: actualFHD 1920, тот же owned save/camera/effects, оба фактора .001, сопоставимая initial thermal state, root visual gate. Для этого helper **снова** меняет только test config/UI после уже выполненного восстановления.
3. Снять новые два SF-окна D с low-overhead guard. Не использовать failed154725 как первое окно и не дописывать его evidence. Optional E uncapped сравнивается только с A; optional F ANGLE60 — с C. B↔D и C↔F — соответствующие patch axes. C↔D меняет backend и APK одновременно и не является такой парой.
4. Проверить candidate audio, point lights/shadows, input, menu/load, HOME/Recents, другую сцену/physics и более длительную стабильность. Native основной backend, ANGLE сохранить. Не обещать 60 или экономию до результатов.
5. После каждого тестового блока остановить игру/погасить экран; по завершении вернуть сегодняшние config и 28UI, проверить 12 saves. Дополнить отчёт/verification/session-state и этот handoff реальными фактами, не данными планируемой группы.

### Команды, только после реального reconnect и успешного восстановления

Из W, PowerShell, с $taskPython из раздела восстановления. Current normal thermala1 уже установлен; rebuild/install для этих команд не требуется.

```powershell
& $taskPython 'work/thermal-power-20261023/device_session.py' launch --label reconnect-d-native-fhd --backend native --save cg22benchp4
```

Launch ждёт End of synchronization, проверяет thermal, запрашивает оба clock factors, делает loaded-before-esc.png и actual FBO assertion. Root обязан посмотреть screenshot/log. Если консоль действительно видна, использовать ESC/KEYCODE111, затем снова проверить; слепой ESC может открыть меню, если load уже скрыл консоль. После wake нужен минимум 2 s foreground/focus для SDL. Fresh loaded PID берёт сам helper.

```powershell
& $taskPython 'work/thermal-power-20261023/device_session.py' measure --label reconnect-d-native-fhd --backend native --seconds 30
```

Это два окна по 30 s; measure после них force-stop/screenOFF. Для E добавлять **--uncapped и к launch, и к measure** с новым label. Для F использовать --backend angle_vulkan без --uncapped и новый label. Не смешивать flags/группы. При дополнительном query/esc/wait использовать отдельные labels и визуально подтвердить world перед measure; они не являются FPS-capture.

Офлайн-команды:

```powershell
& $taskPython 'work/thermal-power-20261023/analyze_device_runs.py' --self-test
& $taskPython 'work/thermal-power-20261023/analyze_device_runs.py' --verification 'work/thermal-power-20261023/root-verification.json' --write --out 'work/thermal-power-20261023/analysis-reconnect-final'
& $taskPython 'work/thermal-power-20261023/restore_today.py' --plan
& ./work/build.ps1 -Variant thermala1
& $taskPython 'work/verify_angleaudit_apk.py' --variant thermala1 --expect-angle present
```

--out должен быть свежим. Default collector --plan безопасен; standalone --run требует свежий реально работающий PID и root ownership. Использовать актуальный D/device_session.py для окон, а не запускать старый full observer поверх SF. Presence Gradle property OXRAY_GPU_PROFILE включает диагностику даже при значении false; обычной сборке property не передавать, CMake OFF проверять verifier.

## Не внедрённые B/C и защищённые риски

- **Новый precise 60 pacer не в APK.** D/pacing/StandaloneFramePacer60.h — отдельный monotonic rational-deadline prototype с <=1 blocking sleep, 100 000 virtual frames, ASan/UBSan и NDK tests. Main loop вызывает FrameMove/seqFrame/input/scripts/часть AI. Ограничив loop, можно снизить их фактическую cadence в быстрых сценах; нельзя назвать это render-only или внедрить вопреки запрету менять AI/physics update frequency.
- Existing config 60 — integer 16 ms, арифметически 62.5FPS при мгновенной работе/идеальном сне. Старый limiter использует paused/scaled TimerGlobal. -unlock_fps отключает gameplay cap и отдельно меняет Java per-window high-refresh vote. Это не независимый переключатель 60/120.
- SDL interval 2 передаётся EGL, но cached getter 2 не доказывает effective 2 или actual 60. На 120Hz возможная номинальная 60, на 60Hz —30; реальные config ranges/driver/compositor ещё требуют измерения. Не добавлять interval 2 как «безопасный cap».
- Combined GL light transforms — только **B fixture**, D/gpu-memory/transform-fixture/audit.md:196 608 cases/589 824 modeled draw snapshots,7→3 products, sanitizer/mutations PASS в трёх конкретных straight-line light blocks. Generic replacement доказанно неэквивалентен при intermediate observers/aliased inputs. Production API/calls не менялись.
- Static luminance vertices: доказаны 1344 B и 3 uploads/frame, но persistent VBO требует ownership/reset/QuadIB fixtures. Общий exact uniform cache и immutable FBO pool требуют generation/range/raw-write/context invalidation; не вставлять как очередной быстрый cache.
- ft diagnostics живут в обычном Android release; trail format/locks продолжаются после verbose window, detached watchdog 1 Hz. Изоляция требует сохранить старую background/heartbeat защиту. Task Workers уже спят в condition wait; TaskManager::Wait помогает/крутится до completion, а newWorkArrived не является completion event. Scheduler сейчас не переписывать.
- Device accelerometer stream связан с orientation callback; при его выключении сохранить orientation и физический controller gyro. Logging flush batching меняет crash-log durability; не устранять диагностику без отдельного контракта.
- Present clear перед full blit — B, пока не доказаны scissor/resize/error/black fallback. Tile discard/объединение passes — C без last-consumer/liveness/hardware evidence.
- **CRT/CTexture reset risk C:** destroy/Unload оставляют scalar deleted handle ненулевым при ранних create/Load guards. Нужен отдельный reset/reuse/context-loss fixture; не добавлять persistent GPU caches поверх непроверенного lifetime.
- Сохранить прежние grass batching, level registration, HOM/menu, comparator, FS mapping, skeleton/task, state/stream, input/overlay/controller/sound/Vorbis fixes. Не сбрасывать их, не повторять как новые находки, не возвращать отвергнутые -gles_stream_orphan/-gles_stream_sync/-gles_state_rebind.
- Повторный MAIN/LAUNCHER поверх живой stopped GameActivity ранее давал ложный crash marker. Для lifecycle return использовать существующую Recents-карточку и проверять Activity+PID; не откатывать engine по одному этому диалогу. Launcher component: `org.openxray.dev/org.openxray.LauncherActivity`; applicationId и Java namespace различаются.
- Не менять качество/разрешение/плотность травы/тени/эффекты ради результата. Не менять global Android/Windows settings, stay-on, brightness policy, clocks, thermal limits и batterystats. Не использовать старые helpers со svc power stayon, даже для «возврата 0».
- При thermal status>=LIGHT(1), unknown/override monitor или unexpected PID/layer loss root прекращает блок, закрывает игру и гасит экран. Нормальный status 0 не означает одинаковую мощность. Отсутствующее устройство — остановка device work, не продолжение в слепом режиме.
- **Не выключать ПК автоматически.** Запрос 22 октября уже исполнен и не относится к этой thermal-сессии.

## Готовое поручение следующему агенту

Продолжи thermal/power задачу OpenXRay Android из R. Сначала прочитай этот handoff и отчёты. Последняя подтверждённая установка — normal thermala1 e54ecc…; Native FHD load успешен, candidate FPS не снят из-за отключения в 15:47, candidate ANGLE не запускался. Устройство отсутствует, восстановление ожидает подключения. При reconnect первым действием выполни restore_today.py: точный сегодняшний user.ltx, 11 673 байта, SHA b129ad…; все 28 persisted UI states и 12 save hashes; подтверждение stopped/screen OFF. Только после успешного восстановления заново создай тестовую FHD/.001/config 60 конфигурацию для candidate D и сними новые окна с low-overhead guard. Старый контроль 22f0… сохранён. Валидные контрольные A/B/C дают 46.053/45.956/40.726 FPS; candidate выигрыш и 60 FPS не доказаны. Не меняй качество, AI/physics cadence, global settings или Git. Pacer, combined transforms, cache и lifetime B/C в production не внедрены. Доведи runtime-проверки, допустимое сравнение и восстановление до подтверждённого результата; W/J оставь неизвестными без валидного измерения мощности.
