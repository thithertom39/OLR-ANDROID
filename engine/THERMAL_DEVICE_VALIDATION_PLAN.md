# OpenXRay Android: план проверки тепла, нагрузки и подачи кадров

Дата: 23 октября 2026. Актуальное состояние — после отключения планшета около **15:47 MSK**. Единственный оператор устройства — root-агент текущей сессии. Автор этого документа проверил исходники helpers и сохранённую evidence offline; ADB не запускал. Аппаратные факты ниже принадлежат root и явно отделены от оставшегося плана.

Цель — сравнить обычную `cordonperf1`, существующую настройку ограничения FPS и кандидат с двумя локальными устранениями лишней работы. Качество, разрешение и игровая сцена должны совпадать. Проценты снижения мощности, температуры и гарантированные 60 FPS заранее не обещаются.

## Сначала восстановление при переподключении

В 15:47 root получил пустой список ADB и `device not found` **до** начала candidate measurement. Группы A/B/C сняты; candidate Native load проверен, но candidate SF и candidate ANGLE ещё отсутствуют. Последняя известная временная конфигурация устройства: FHD 1920×1080, setting 60, time_factor_single/al_time_factor 0.001; launcher Native/FHD, unlock=false. **Восстановление пользовательской конфигурации и UI остаётся pending.** После отключения нельзя заново подтвердить PID/экран или объявить успешный cleanup.

Первое действие root после появления устройства — восстановить сегодняшние настройки, а не запускать новую FPS серию. `D/restore_today.py` проверен root: default/`--plan` только читает локальные inputs и не вызывает ADB; `--run` явно выполняет восстановление. Точные команды:

```powershell
& 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' 'C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/restore_today.py' --plan
& 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' 'C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/restore_today.py' --run
```

Вторая строка — только root при реальном reconnect, не автоматическое продолжение сейчас. Helper возвращает точный сегодняшний user.ltx (1728×1080, AAoff, limits501/501, обычное время10), восстанавливает/проверяет все **28** исходных UI состояний и сверяет **12** сохранённых файлов. Preferences сохраняются launcher только через Start Game: helper кратко запускает игру без загрузки save, останавливает процесс, возвращает конфиг повторно, перезапускает launcher для проверки persistence и завершает package stopped/screen OFF. При несовпадении protected UI прекращает выполнение, не исправляя остальные controls вслепую. Ни global settings, ни APK, ни save contents он не меняет.

Считать восстановление завершённым только при `restoration_passed=true`, `all_28_persisted_ui_states_equal_first_snapshot=true`, `cleanup_exact_config_verified=true`, `final_stopped_and_screen_off=true` и совпавших 12 save hashes в новом `restore-today-*/result.json`. Пока такой успешной записи нет. Helper не откатывает APK: установленный candidate и решение о его дальнейшей приёмке — отдельные вопросы. Только после восстановления и нового сопоставимого preflight root может вернуться к отложенным D/F тестам.

## Фиксированные условия и сохранность

Обозначения:

- W: `C:/Users/User/Documents/Codex/2026-10-11/gjvyb`.
- R: `W/work/openxray-build`.
- D: `W/work/thermal-power-20261023`.
- Устройство: Galaxy Tab S9, SM-X710, serial `R52X40ES4LE`; приложение `org.openxray.dev`.

**Сегодняшний оригинал — D/device-baseline-132003.** В нём сохранены конфиг, savedgames, package/activity/power/thermal/battery state и manifest. `user.ltx`: 11 673 байта, SHA256 `b129ad651af57f7191734fe91dc98a80d9a1e85931e978a826bccbf94dd568b8`. В нём gameplay/menu limits 501/501, `rs_v_sync off`, `r2_aa off`, `render_scale 1`, `time_factor_single 10`, `vid_mode 1728x1080 (0Hz)`. Восстанавливать именно эту копию, а не вчерашние 11 703 байта.

APK-контроль `W/outputs/openxray-soc-cordonperf1-arm64.apk`:

`22f0ec05d00ee25ad9f62a4aafcbddc99d92eee98ca92e8f4a93d0d3e5738df2`

Кандидат `W/outputs/openxray-soc-thermala1-arm64.apk`:

`e54eccbedfa879cc3c9efedf4c492c31f50de84b8a759fd33c1f9f195c02b4dc`

Кандидат **установлен** root в `D/install-thermala1-154431`: установленный SHA совпал, UID **10361** сохранён, user.ltx установкой не изменён и все **12 save files** побайтово сохранились. Это installation proof, не performance/runtime acceptance; более ранняя `install-thermala1-154332` закончилась без установки. Native FHD load проверен в `D/device-runs/candidate-native-d-fhd-154511-255525/loaded-before-esc.png`: правильный мир без консоли. Следующая measure попытка `candidate-native-d-fhd-154725-677461` завершилась `device not found` до SF.

Кандидат содержит только удаление неиспользуемого jitter texgen в `accum_point` и повторной установки нулевой OpenAL listener velocity. Новый pacer, swap interval2, изменения AI/physics, shaders или качества в него не входят. Контрольный APK сохраняется как самостоятельный файл и не пересобирается из изменённого дерева под старым именем.

Первичный cold preflight: Thermal Status 0, override false, USB powered true, battery level 37/100. Thermal raw AP=21.3, SKIN=22.4, BAT=22.1; battery dump raw temperature=221 и current now=318. Здесь это исходные поля, а не самостоятельно подтверждённые единицы. USB charging/battery current **не равен полной мощности устройства**.

Запрещены любые записи глобальных Android settings, изменение stay-on, системной частоты дисплея, CPU/GPU clocks, thermal policy/limits, brightness policy, сброс batterystats. В этом плане нет таких команд. Допустимы чтение состояния, изменение только приложения/его тестовой конфигурации и управление экраном для коротких блоков. Не оставлять игру работающей/экран включённым во время анализа, сборки и отдыха.

## Матрица сравнений

| Группа | APK | Backend | Gameplay setting | Текущий статус / назначение |
|---|---|---|---|---|
| A | cordonperf1 | Native GLES | 501 + `-unlock_fps` | Два FHD SF окна сняты; основной uncapped контроль |
| B | cordonperf1 | Native GLES | 60, unlock выключен | Два FHD SF окна сняты; существующая политика |
| C | cordonperf1 | ANGLE Vulkan | 60, unlock выключен | Два FHD SF окна сняты; root просмотрел game-after около 15:43: правильная дорога/мир, консоли нет |
| D | thermala1 | Native GLES | 60, unlock выключен | Установка и правильный Native FHD load подтверждены; **SF pending после reconnect/restoration-first** |
| E, optional | thermala1 | Native GLES | 501 + `-unlock_fps` | Ещё не снят; патч A/B против A при одинаковом uncapped режиме |
| F, optional | thermala1 | ANGLE Vulkan | 60, unlock выключен | **Ещё не запускался**; патч A/B против C, отдельный candidate ANGLE smoke |

Буква группы D не означает новый limiter: это APK-кандидат со **старой** настройкой 60. A/B меняет политику подачи кадров; B/D изолирует две локальные правки при прежней политике. C/D меняет одновременно APK и backend и не является строгой парой для вывода об эффективности патча. Native — основной backend; ANGLE результаты сохраняются отдельно.

В текущем коде `rs_fps_limit 60` означает integer `1000/60 = 16 ms`: арифметический потолок при мгновенной работе/идеальном сне — 62.5 FPS, а не точные 60. При `-unlock_fps` gameplay limiter целиком игнорируется. Выключение unlock дополнительно убирает Java high-refresh vote, поэтому фактическая частота панели — обязательный read-only контроль и возможный confound. `rs_v_sync on` на панели 120 Hz сам по себе не даёт cap60. Реальный SF cadence всегда важнее названия режима.

Никакой новый sleep/pacer в production не вводится. Render loop вызывает `FrameMove/seqFrame`, включая frame-driven input, object/script и часть scheduler updates. Ограничение loop может уменьшить число этих вызовов в секунду в быстрой сцене; это нельзя назвать независимым render-only изменением или скрыть от пользовательского запрета ограничивать частоту игровой логики. Физика имеет fixed-step accumulator, но её взаимодействие с dt/catch-up и AI responsiveness тоже требует проверки. Сравнение существующих настроек — отдельный наблюдаемый tradeoff, не основание узаконить новый B.

## Условия валидного окна

Для всех FPS окон одновременно обязательны:

1. Один owned `cg22benchp4`, полностью загруженный мир на Кордоне, одинаковые камера/направление, без движения/поворотов/выстрелов/NPC-взаимодействия во время окна. Reference camera из прошлой сцены: `[-55.414333,-2.253043,-64.284225,0.928497,-0.231284,0.290519]`. В normal APK без camera telemetry численную точность 5 cm/0.25° не заявлять по одному скриншоту; фиксировать тот же save, отсутствие ввода и визуальное соответствие.
2. Настоящий engine FBO **1920×1080**, render_scale 1, прежние трава/тени/материалы/эффекты/AA. Теперь helper явно пишет `vid_mode 1920x1080 (0Hz)` в test config и утверждает последнее фактическое FBO 1920×1080 из свежего `CHW::Present` log. Одного названия FHD в launcher недостаточно: ранняя попытка фактически осталась 1728×1080 и исключена. Physical drawable 2560×1600 допустима; это не подмена render resolution. Исходные 1728×1080 вернуть **первым действием при reconnect**.
3. `time_factor_single=0.001` в тестовом конфиге; **после load** подтверждены оба запроса `time_factor_single` и `al_time_factor=0.001`. SAV/net_import может заменить второй фактор. Один и тот же save недостаточен. Не менять Device time factor, AI или physics settings. Солнце, тени и состав освещения визуально сопоставимы; несовпадение — исключение окна.
4. Консоль и меню **визуально закрыты** до начала и после конца окна; только мир. `ExecuteCommand confirmed` и отправленный ESC не являются такой проверкой. По loading/menu/console FPS результаты не принимать.
5. Свежие PID, live renderer/реальный backend и APK hash подтверждены; PID/layer/config непрерывны. При crash/новой Activity/context loss не продолжать старое окно автоматически.
6. Thermal Status 0 без override, сопоставимые исходные AP/BAT/SKIN readings с тех же источников/имён. До сравнения заранее зафиксировать raw диапазоны/допуски; не выбирать их после просмотра FPS. Разный cold/warm старт нельзя компенсировать одной отметкой status0.
7. Неизменные USB/charging context, кабель/charger, ориентация, ambient условия; battery level и actual display/brightness state записаны. Brightness и display читаются из dumpsys, не выставляются системными командами. Изменение auto-brightness/refresh в окне помечать как confound.

Между короткими блоками приложение остановлено/приостановлено, экран OFF. Отдых продолжать до сопоставимого исходного thermal raw состояния, а не только фиксированное число секунд. Для строгой пары чередовать A/B/A/B или B/D/B/D с одинаковым порядком загрузки/короткого прогрева. Если выполнена лишь одна серия двух 30-секундных окон, так и писать: это короткое сравнение, не sustained thermal acceptance.

## Проверенные root-команды и поведение helper

Команды ниже предназначены **только root-оператору**, как воспроизводимый план после restoration-first. Сейчас устройство отключено; эти блоки не являются выполненными D/F действиями. Каждый label — lowercase ASCII/hyphens; helper создаёт новый timestamped каталог. В PowerShell:

```powershell
$thermalWorkspace = 'C:/Users/User/Documents/Codex/2026-10-11/gjvyb'
$thermalRepo = Join-Path $thermalWorkspace 'work/openxray-build'
$thermalEvidence = Join-Path $thermalWorkspace 'work/thermal-power-20261023'
$thermalPython = 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$thermalSession = Join-Path $thermalEvidence 'device_session.py'
$thermalAdb = Join-Path $thermalWorkspace 'work/android-sdk/platform-tools/adb.exe'
Set-Location $thermalWorkspace
```

Host build/verifier кандидата уже прошли; точные команды для воспроизведения после согласованной фиксации inputs:

```powershell
& (Join-Path $thermalWorkspace 'work/build.ps1') -Variant thermala1
& $thermalPython (Join-Path $thermalWorkspace 'work/verify_angleaudit_apk.py') --variant thermala1 --expect-angle present
Get-FileHash (Join-Path $thermalWorkspace 'outputs/openxray-soc-cordonperf1-arm64.apk') -Algorithm SHA256
Get-FileHash (Join-Path $thermalWorkspace 'outputs/openxray-soc-thermala1-arm64.apk') -Algorithm SHA256
```

Verifier проверяет certificate/package, ARM64/16K alignment, зависимости, GL dispatch, source-input hashes; это не runtime/energy приёмка. `thermala1-apk-report.json`: 29 ARM64 libraries, matching certificate, normal build. GPU diagnostic property в обычной сборке должна **отсутствовать**: существующее `-POXRAY_GPU_PROFILE=false` тоже включило бы диагностику по наличию property. Не использовать `build_gpu_diag.ps1` для этих normal APK сравнений. Готовый контроль не перезаписывать и не пытаться перепроверить его старые source hashes против уже изменённого дерева как новую сборку.

Актуальный `device_session.py launch` сначала force-stop только этого package, делает test user.ltx **из сегодняшнего BASE**, меняя `time_factor_single`, `rs_fps_limit`, `rs_fps_limit_in_menu` и `vid_mode`; выбирает backend/unlock/FHD через launcher UI и загружает save. Затем до 90 s опрашивает thermal/log на `End of synchronization A[1] R[1]`, выполняет запросы обоих факторов и сохраняет loaded-before-esc.png. Успех команды/истечение timeout не заменяет визуальный gate готового мира. После action проверяется actual FBO по свежему log.

`wait` будит экран, даёт 2 s foreground и ещё 25 s загрузке, сохраняет screenshot. `esc` будит, ждёт 2 s, отправляет **KEYCODE_ESCAPE 111** и сохраняет результат. **Все эти actions в finally выключают экран.** Пока экран OFF, SDL может приостановить продолжение загрузки. Повторять wait только по визуальной необходимости; не отправлять ESC вслепую во время staged loading. Если консоль уже закрыта, ESC способен открыть меню — action тогда пропустить.

Актуальный `measure` сам не запускает/не перенастраивает игру: будит текущую Activity, ждёт 2 s, сохраняет game-before, делает **один context snapshot вне FPS**, запускает `--thermal-only` safety collector и ждёт ready handshake. SF начинается только при `ready=true` и измеренном first-sample wall ≤200 ms. Далее идут **два** SF окна по `--seconds`, game-after и второй context snapshot вне FPS; сохраняются host monotonic границы SF и actual FBO. В finally останавливает приложение и выключает экран. Параметры backend/uncapped у measure — записанная метка, не runtime проверка backend. Изображение/камеру/консоль скрипт не распознаёт автоматически.

Группы A/B/C уже выполнены. Их команды ниже оставлены для воспроизведения **только с заранее проверенным установленным original cordonperf1 hash**. Сейчас установлен candidate: один control label не делает его контрольной сборкой. Не запускать эти блоки как продолжение отключённой сессии или перед restoration-first.

### A — исходный Native uncapped

```powershell
& $thermalPython $thermalSession launch --label a-control-native-uncapped --backend native --uncapped --save cg22benchp4
& $thermalPython $thermalSession wait --label a-control-native-uncapped-wait --backend native --uncapped
```

Просмотреть loaded-before-esc/after-wait. После завершённой загрузки, **только если консоль видна**:

```powershell
& $thermalPython $thermalSession esc --label a-control-native-uncapped-esc --backend native --uncapped
```

Просмотреть after-esc и выполнить проверки факторов/сцены ниже. Только после визуального gate:

```powershell
& $thermalPython $thermalSession measure --label a-control-native-uncapped --backend native --uncapped --seconds 30
```

### B — тот же контроль Native, существующая настройка 60

После отдыха/thermal matching:

```powershell
& $thermalPython $thermalSession launch --label b-control-native-existing60 --backend native --save cg22benchp4
& $thermalPython $thermalSession wait --label b-control-native-existing60-wait --backend native
```

По изображению при необходимости выполнить ESC, затем отдельно подтвердить закрытый мир, clock и actual FBO:

```powershell
& $thermalPython $thermalSession esc --label b-control-native-existing60-esc --backend native
& $thermalPython $thermalSession measure --label b-control-native-existing60 --backend native --seconds 30
```

Последние две строки **не запускать одним слепым batch**: между ними обязательный просмотр after-esc. Тот же gate действует для C/D/E.

### C — контроль ANGLE Vulkan, существующая настройка 60

```powershell
& $thermalPython $thermalSession launch --label c-control-angle-existing60 --backend angle_vulkan --save cg22benchp4
& $thermalPython $thermalSession wait --label c-control-angle-existing60-wait --backend angle_vulkan
```

После готовности, ESC только если нужен, визуальный gate, затем measure:

```powershell
& $thermalPython $thermalSession esc --label c-control-angle-existing60-esc --backend angle_vulkan
& $thermalPython $thermalSession measure --label c-control-angle-existing60 --backend angle_vulkan --seconds 30
```

### D — кандидат Native, существующая настройка 60

Установка уже выполнена и подтверждена в `D/install-thermala1-154431/result.json`; повторять её ради продолжения D не нужно. Сохранены UID 10361 и 12 save files, прежний APK лежит отдельно. Правильный Native FHD мир без консоли root просмотрел около 15:47 в `candidate-native-d-fhd-154511-255525/loaded-before-esc.png`. Новых SF интервалов candidate нет: следующая measure попытка упала на первом thermal read из-за отключения устройства.

Сейчас не выполнять продолжение этого блока. При reconnect **сначала restore_today.py и проверка восстановления**, затем отдельно согласованное возвращение к test config, новый preflight/отдых и fresh load:

```powershell
& $thermalPython $thermalSession launch --label d-candidate-native-existing60 --backend native --save cg22benchp4
& $thermalPython $thermalSession wait --label d-candidate-native-existing60-wait --backend native
```

После загрузки/clock/FBO, ESC при необходимости и визуальный gate:

```powershell
& $thermalPython $thermalSession esc --label d-candidate-native-existing60-esc --backend native
& $thermalPython $thermalSession measure --label d-candidate-native-existing60 --backend native --seconds 30
```

Optional E повторяет A с label `e-candidate-native-uncapped` и теми же `--backend native --uncapped`; optional F повторяет C с candidate label. Для настоящего чередования APK root возвращает сохранённый original APK через install -r и проверяет установленный hash; «до» из другой температуры/даты не превращается в строгую A/B/A/B серию.

## Clock, display и console review вне FPS окна

`console.py` умеет запросы обычных команд, автоматически посылает ESC после каждой и сохраняет свежие log/screenshot; load/start он специально не закрывает автоматически. Актуальный session helper уже включает оба запроса после load, а отдельный `query` сам будит экран, даёт 2 s foreground и снова гасит экран в finally. Для уже загруженного мира с закрытой консолью, только после restoration-first и нового тестового запуска:

```powershell
& $thermalPython $thermalSession query --label root-clock-query --backend native
```

Для соответствующей группы указывать её `--backend` и при необходимости `--uncapped`; это provenance, а не смена текущего backend. Результат/log/after-query.png лежат в новом D/device-runs/root-clock-query-*. Проверить **значения в log**, а не только execution_confirmed. После блока просмотреть изображение: консоль/меню отсутствуют, сцена та же. Если что-либо не подтверждено, не начинать measure. Display/brightness context root читает отдельно через `dumpsys display`/`dumpsys power`, сохраняет в evidence и не меняет системные параметры. Не включать UI/debug overlays ради FPS окна.

## Collector CLI, safety и где лежат результаты

Read-only power/thermal collector — D/telemetry/power_samples.py. Текущий host proof `telemetry/offline-proof.json`: **36 tests PASS**; offline analyzer — **13 checks PASS** в `analysisresults-self-test.json`. Mocked/captured inputs, default plan и analyzer self-test не вызывают ADB. Это не аппаратный результат. Повтор host проверки при изменении кода:

```powershell
& $thermalPython -m unittest discover -s (Join-Path $thermalEvidence 'telemetry') -p test_power_samples.py -v
& $thermalPython (Join-Path $thermalEvidence 'telemetry/power_samples.py') --plan --thermal-only --seconds 30 --period 5
& $thermalPython (Join-Path $thermalEvidence 'analyze_device_runs.py') --self-test
```

**Исправленная методика observer:** ранний full inventory/per-thread sampler занимал 9.313–9.953 s на proc/sysfs sample и сделал серию `control-native-uncapped-141937-672886` невалидной для performance/energy сравнения. Slim context с 6 readable paths всё ещё занимал ≈953 ms; попытка `control-native-uncapped-valid-144047-753568` была остановлена readiness gate до SF. Имена `valid` и `fhd` не являются доказательством. Эти данные сохранены как ошибка наблюдателя, а не baseline.

В FPS окне теперь только **`--thermal-only`**: `dumpsys thermalservice` и `pidof`, без battery/proc/stat/thread/sysfs reads. В успешных A/B/C первые flushed samples заняли соответственно **125 / 109 / 93 ms**, ready=true, overhead_target_exceeded=false. Это реальные времена **первого** sample, не гарантия стоимости каждого будущего опроса. Context/battery/frequency/proc snapshots выполняются отдельно по одному до и после FPS; это не render-thread CPU time и не синхронная CPU выборка на каждый SF кадр.

Root-only standalone safety capture, если нужен отдельно от coordinator: получить **свежий единственный** PID текущей уже работающей игры и новые JSONL/ready paths. Не запускать второй collector параллельно встроенному в measure. Только после restoration-first и нового тестового запуска:

```powershell
$thermalGamePid = (& $thermalAdb -s R52X40ES4LE shell pidof org.openxray.dev).Trim()
if ($thermalGamePid -notmatch '^\d+$') { throw 'Expected one fresh running game PID' }
$thermalSamplePath = Join-Path $thermalEvidence ('telemetry/root-extra-' + (Get-Date -Format 'HHmmss-fff') + '.jsonl')
$thermalReadyPath = $thermalSamplePath + '.ready.json'
& $thermalPython (Join-Path $thermalEvidence 'telemetry/power_samples.py') --run --thermal-only --serial R52X40ES4LE --pid $thermalGamePid --seconds 30 --period 5 --out $thermalSamplePath --ready-file $thermalReadyPath
```

Это blocking команда safety collector; для параллельного запуска SF root владеет отдельным процессом и ждёт ready handshake, как делает coordinator. Не трактовать последующий старт SF после завершения этой строки как monitored capture. `ready=true` означает flushed first sample; root дополнительно проверяет stop=false и sample_host_wall_ms ≤200 до SF.

Без `--thermal-only`, с `--seconds 0` получается один context snapshot, **только вне FPS окна**. Актуальный coordinator использует заранее известные 6 readable fixed paths через `--readable-paths`, а не новый полный discovery. `--thermal-only` несовместим с readable-paths/inventory/threads. Out создаётся эксклюзивно. Units/signs остаются raw/unknown, missing/denied — null с причиной; `device_power_w`/`energy_j` всегда null. CPU ticks в context требуют измеренного CLK_TCK; interval thermal-only вообще не собирает CPU ticks/starttime. PID identity там ограничена pidof presence, дополнительно SF проверяет PID/layer на обоих концах. Frequency snapshots/gpubusy tuple не превращаются в средние utilization/power.

Сам power collector **только читает** и прекращает telemetry при обнаружении Thermal Status >= LIGHT (1), unknown/override safety monitor, PID exit/change/reuse или неполном bundle. Он не закрывает игру. Встроенный coordinator при nonzero sampler exit прерывает SF child и в finally force-stop/screen-OFF. При ручном collector root обязан немедленно остановить блок и выполнить:

```powershell
& $thermalPython $thermalSession stop --label safety-abort
```

Интервал sampling 5 s и command latency ограничивают скорость обнаружения; это не hard realtime thermal protection. `launch/wait/esc` проверяют thermal до и читают после, но не ведут continuous monitor внутри всего блока. При любом ухудшении состояния — короткий stop/OFF, затем отдых и новый preflight, без повышения thermal limits.

**Исправленный coordinator:** thermal-only sampler запущен на `2*seconds+90`, ready gate обязателен, а `monitor_child` теперь прерывает живой SF child при **любом** завершении sampler, включая exit0. Прежняя возможность продолжить SF после normal sampler exit устранена. Всё равно сохранять/проверять monotonic границы SF, thermal samples и отсутствие in-window stop: requested duration не заменяет evidence. После завершённых SF окон штатный force-stop может дать sampler exit2; оценивать по timeline и `sampler_cleanup_after_sf_complete`, а не смешивать cleanup с in-window crash/thermal abort. В A/B/C analyzer принял именно post-SF cleanup; thermal stop не прощается этим исключением.

SF collector используется прежний, без нового parser algorithm:

```powershell
& $thermalPython (Join-Path $thermalWorkspace 'work/cordon-gpu-20261022/telemetry/collect_present.py') --run --label root-sf-extra --seconds 30 --period 0.5 --screen --scene-note 'Root visually confirmed closed console, loaded cg22benchp4, same camera/clock/lights, actual FBO1920x1080; power collected separately'
```

Standalone SF collection само не содержит thermal stop: запускать только под root-owned monitoring. Основной путь — `device_session.py measure`, который уже запускает SF и power. Не создавать второй конкурирующий SF capture без причины.

Session outputs — D/device-runs/label-timestamp: result.json, engine.log, power.jsonl/stdout, screenshots, thermal-before/after, power-final и pid-final. Actual SF outputs остаются в **старой папке parser/collector** `W/work/cordon-gpu-20261022/captures/label-r1-*` и `label-r2-*`; это новые captures текущей даты, не результаты предыдущей сессии. Их точные пути есть в sf-run*.stdout. Ссылки на обе группы evidence добавить в root results manifest.

`capture.json` / `present-analysis.json`: требуются same process, same layer, unchanged config bytes, valid continuous ring history и неизменный refresh period. Исторический priming anchor исключается; gaps не фильтруются молча. FPS = число интервалов / их сумма; P95/P99 nearest rank; 1% low — reciprocal среднего худшего ceil(1% N), сохранять tail count. SF actual-present time не равен CPU/GPU duration, а отдельно взятый sysfs sample не равен средней загрузке. SF/power clocks и windows не объявлять точно синхронными по каждому кадру без дополнительного evidence.

## Приёмка и восстановление

Для пары B/D и optional A/E нужны одинаковая политика и сопоставимые исходные условия. Сначала visual/audio smoke: неподвижный мир и point lights/shadows, затем вне FPS окон короткое перемещение/поворот со spatial audio, menu/load, HOME→существующая карточка Recents. Отдельно записать crash/lifecycle/GL/audio errors. Не исправлять образовавшийся второй Launcher созданием новых intents вслепую: старая сессия уже показала ложный process-died dialog при живой остановленной game Activity.

Успех runtime smoke и две короткие серии не доказывают power/thermal improvement. При USB charging battery current/voltage, raw frequency, температура и FPS дают разные наблюдения. Меньшая CPU работа при том же кадре/картинке — полезное структурное доказательство; общие W/J, проценты battery savings и sustained cooling из этих counters не вычислять. В GPU-bound сцене ниже60 старый cap60 может вообще не создавать idle slot; это результат, а не повод снижать качество/AI/physics.

Восстановление больше не отложено до конца следующего measurement: после disconnect это **первое действие root при reconnect**, по `D/restore_today.py` и критериям в начале документа. Helper возвращает точные сегодняшние D/device-baseline-132003/user.ltx и проверяет обратным pull SHA/bytes; использует **самый первый полный snapshot 28 UI состояний**, а не сокращённый restore-selection.json. Все 12 сохранённых файлов проверяются, а не перезаписываются. Оригинальные quicksave/cg22benchp4 не сохранять поверх себя; extras не удалять wildcard. Private SharedPreferences XML побайтово не сохранён/не проверен, поэтому scope — все 28 UI states и их persistence после нового процесса.

После helper нужны его успешный result.json, final PID/power и совпавшие hashes. До reconnect эти проверки невозможны; **restoration pending, успех не заявлен**. Текущее live состояние экрана/процесса неизвестно: последний успешный launch helper отправил screen-OFF, но во время последующего disconnect повторно это не прочитано. Кандидат остаётся установленным, его SF/ANGLE/audio/lifecycle acceptance не завершена. Решение о возврате original APK или продолжении candidate тестов root принимает отдельно, сохраняя исходный APK и данные. Глобальные Android settings не меняются.

## Фактическая аппаратная запись root

Актуализация по сохранённым results и последним прямым сообщениям root (приоритет над stale `session-state.json` от 15:20). Исходный launch 13:46 был только подготовкой; актуальные FPS серии — следующие:

| Группа | Завершённая session в D/device-runs | SF FPS r1 / r2 | P95 r1 / r2, ms | P99 r1 / r2, ms | 1%low r1 / r2, FPS | Статус |
|---|---|---:|---|---|---|---|
| A control Native uncapped | `control-native-a-fhd-150518-617589` | 46.082 / 46.023 | 33.333 / 25.000 | 33.333 / 33.333 | 30.000 / 30.000 | Два окна ≈30 s, technical eligible; root подтвердил мир без консоли до/после |
| B control Native existing60 | `control-native-b-fhd-151738-316785` | 46.126 / 45.786 | 33.333 / 33.333 | 33.333 / 33.333 | 30.000 / 30.000 | Два окна ≈30 s, technical eligible; root подтвердил мир без консоли до/после |
| C control ANGLE existing60 | `control-angle-c-fhd-152952-066113` | 41.718 / 39.734 | 33.333 / 33.333 | 33.333 / 33.333 | 28.364 / 25.714 | Два окна ≈30 s, technical eligible; root просмотрел game-after около 15:43: та же дорога, консоли нет |
| D candidate Native existing60 | launch `candidate-native-d-fhd-154511-255525`; failed measure `candidate-native-d-fhd-154725-677461` | **нет** | — | — | — | Правильный Native FHD load/no console подтверждён около 15:47; отключение до SF, measurement pending |
| F candidate ANGLE | **нет** | **нет** | — | — | — | Не запускался, pending |

Контрольные A/B/C использовали original `22f0…38df2`, FBO 1920×1080, drawable 2560×1600, оба queried time factors 0.001, тот же save/reference camera. Parser показывает refresh period 8,333,333 ns (120 Hz), без изменения внутри окон. Readiness 125 / 109 / 93 ms соответственно. SF median ≈25 ms. Это отдельные короткие наблюдения: Native existing60 не превратил тяжёлый мир в 60 FPS. Нет candidate before/after численного результата и нет доказанной экономии.

Связь session → новые captures в `W/work/cordon-gpu-20261022/captures`:

- A: `control-native-a-fhd-r1-20261023-150524-738508`, `control-native-a-fhd-r2-20261023-150602-601258`.
- B: `control-native-b-fhd-r1-20261023-151744-439130`, `control-native-b-fhd-r2-20261023-151822-299638`.
- C: `control-angle-c-fhd-r1-20261023-152958-248524`, `control-angle-c-fhd-r2-20261023-153036-312546`.

Evidence-сводка: `D/analysisresults-control-abc.json/.md`; actual parser output — capture.json/present-analysis.json в этих captures. `D/root-verification.json` содержит камеры и A/B visual attestations, но ещё хранит устаревший C after-pending: более позднее прямое подтверждение root около 15:43 снимает именно этот visual pending; сам JSON здесь не редактировался.

Thermal status во всех трёх guard сериях оставался 0, USB powered=true до/после. Raw AP от initial guard до последнего полностью попавшего в SF sample: A 27.7→48.0, B 28.4→48.0, C 35.4→47.8; terminal C 50.1 уже нельзя подменять last-in-window значением. Нельзя выдавать эти raw readings за доказанное охлаждение или полную мощность. Начальный AP отличается: A/B разница 0.7 raw превышает принятую analyzer policy 0.5 raw, C стартовал заметно теплее. Weather/absolute clock equality не установлена одними save+factor 0.001; weather_clock_id отсутствует. Поэтому analyzer не допускает строгой patch/energy пары, даже при технически пригодных individual captures.

Whole-process CPU context между внешними snapshots: A 86.62 s, B 87.44 s, C 89.67 s при измеренном CLK_TCK 100. Это сумма всех process threads между endpoints **вне** SF, а не render-thread milliseconds/синхронное окно SF и не device CPU utilization или энергия. In-window thermal-only вообще не измерял CPU/GPU power. W/J остаются неизвестными.

Исключённые попытки сохранены: full observer `control-native-uncapped-141937-672886` с ≈9.9 s overhead/sample; abort `control-native-uncapped-valid-144047-753568` с ≈953 ms readiness и нулём SF; FHD-labelled ранний launch `control-native-a-144957-847914`, фактически 1728×1080. Их числа не включать в основной baseline и не смешивать с исправленными A/B/C.

Root installation proof `D/install-thermala1-154431/result.json`: candidate `e54e…b4dc` установлен, UID 10361 и 12 saved files сохранены; `runtime_verified=false` не переименовывать в полную приёмку. Root просмотрел candidate `loaded-before-esc.png` около 15:47: правильный мир/дорога без консоли. Measure result содержит ошибку первого `dumpsys thermalservice`: `device not found`; никаких candidate SF окон не было. Root сообщил пустой ADB список. После disconnect live game/screen state неизвестно, новые действия/успешное восстановление не заявлены.

Оставшееся: **restore_today.py первым при reconnect**, доказанная обратная проверка user.ltx / 28 UI states / 12 saves / stop+OFF; затем отдельно возможное продолжение D, candidate ANGLE smoke, audio/lifecycle и сопоставимая повторная B/D серия. Не объявлять обычный candidate полностью принятым, restored или более холодным по одному успешному load.
