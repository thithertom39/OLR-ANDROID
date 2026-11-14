# OpenXRay Android: аудит нагрева и энергозатрат

Дата: 23 октября 2026 года.

**Подтверждены и включены в `thermala1` два небольших устранения лишней CPU-работы: повторная передача нулевой скорости слушателя OpenAL и вычисление неиспользуемой матрицы jitter для point light.** Сравнение реальных извлечённых функций, проверки чувствительности тестов, ASan/UBSan, Android NDK и общая сборка прошли. Разрешение, шейдеры, геометрия, свет, тени, эффекты, управление и частота обновления игровой логики этими правками не изменены.

**Экономия энергии, снижение температуры и выигрыш двух новых A-правок пока не установлены.** Actual APK binary proof пройден; контрольные аппаратные серии A/B/C старой `cordonperf1` получены и описаны ниже. Кандидат установлен и загрузил игровую сцену Native GLES; дальнейшие замеры прерваны отключением устройства. Полная аппаратная приёмка и восстановление исходных настроек ещё не выполнены. Наличие APK, successful binary proof и контрольных запусков не заменяет проверку кандидата на устройстве.

## Сохранность исходников и границы работы

Workspace W: `C:/Users/User/Documents/Codex/2026-10-11/gjvyb`.

Репозиторий R: `W/work/openxray-build`. Evidence D: `W/work/thermal-power-20261023`. Пути `src/...` и `android/...` далее отсчитываются от R, короткие пути evidence — от D.

Перед изменениями сохранено фактическое рабочее дерево с предыдущими незакоммиченными исправлениями и нужными JNI-библиотеками: `D/source-baseline.zip`, **11 720 файлов, 53 333 478 байт ZIP**. SHA256 архива: `d4f8f1a581c8dbb8a91aacbbce4a12cad6925db6c7db21098164bcedd8e6194b`. [Манифест](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/baseline-manifest.json) содержит хэши исходников; ключи и build caches исключены. Это снимок текущего дерева, а не замена его чистым Git HEAD. Reset, commit и push в эту работу не входят.

Старый контроль сохранён отдельно: `outputs/openxray-soc-cordonperf1-arm64.apk`, SHA256 `22f0ec05d00ee25ad9f62a4aafcbddc99d92eee98ca92e8f4a93d0d3e5738df2`. Новая сборка его не перезаписывает. Сейвы и конфиги устройства сохраняет root; все обращения к устройству принадлежат ему. Подагенты выполняли исходный аудит и локальные проверки без ADB.

Root-доступ, изменение глобальных настроек Android или Windows, фиксация CPU/GPU-частот и изменение thermal policy не используются. WSL2 Ubuntu 24.04 доступна и фактически использована для sanitizer-проверок; недоступность Linux-среды не является ограничением этой работы.

Сохранены прежние grass batching, уникальная регистрация уровня, HOM/menu, корректный comparator, FS mapping, skeleton/task, stderr/UI trace, sound/Vorbis, overlay MOVE и SDL controller-close исправления. Новые A-правки не переписывают эти пути. Не меняются AI budgets, physics fixed step/max substeps, квесты, игровые скрипты и частота callback-обновлений.

Основа отчёта — три независимых аудита:

- [CPU, lifecycle, allocations](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/cpu-lifecycle/audit.md).
- [Render loop и frame pacing](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/pacing/audit.md), [отдельный B-прототип и swap interval 2](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/pacing/B_PROTOTYPE_AND_SWAP2.md).
- [GPU, uploads и время жизни ресурсов](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/gpu-memory/audit.md).

## Что вошло в обычную APK

Категория **A** означает локальное устранение доказанно лишней работы с сохранением наблюдаемого результата функции. **B** требует отдельной реализации, проверки времени жизни или явно разрешённого эксперимента. **C** отложена из-за риска, отсутствия измеренной значимости либо более широкого изменения архитектуры. Категория не задаёт величину экономии энергии.

| Категория | Кандидат | Фактический статус | Что доказано и что ещё требуется |
|---|---|---|---|
| A, CPU | Нулевая `AL_VELOCITY` в `CSoundRender_CoreA::update_listener` | Реализован; одна строка удалена | Инициализация каждого контекста сохранена; один повторный API-вызов и его публикация свойств убраны на update. Реальный звук и энергозатраты проверяет root. |
| A, CPU в render path | Неиспользуемый jitter texgen в `CRenderTarget::accum_point` | Реализован; две строки удалены | Два умножения матриц и identity убраны на вызов point light; используемые команды и данные равны в fixture. Число таких вызовов на реальный кадр и временная доля не измерены. |
| B, standalone | Точный монотонный pacer 60 Hz; отдельно swap interval 2 | Только prototype/fixtures в D; **в APK отсутствует** | Алгоритм расписания проверен; ограничение основного loop может менять cadence игровой логики. Без отдельного разрешения на этот tradeoff не включать. |
| B, CPU/audio | Exact cache позиции/ориентации OpenAL | Не реализован | Повторные равные записи существуют; нужны per-context invalidation и проверка commit semantics. |
| B, diagnostics | Изоляция обычного Android `ft` trail/watchdog | Не реализован | Подробные checkpoints и watchdog остаются в обычном release; требуется сохранить контракт диагностики и background-защиту. |
| B, Android | Исключение неиспользуемых device accelerometer samples | Не реализован | Поток callback/JNI есть, но он также обслуживает orientation; физический controller gyro должен сохраниться. |
| B, logging | Пакетирование file flush | Не реализован | Flush каждой строки подтверждён. Меняется сохранность последних crash logs; нужна отдельная политика и проверка. |
| B, GPU resource | Неизменные luminance vertices | Не реализован | Доказаны одинаковые 1344 байта и три stream upload ranges на кадр; нужны VBO ownership/reset fixtures. |
| B, transforms | Совместная установка W/V/P перед draw | Не реализован | В отдельных последовательных местах 7 matrix products дают три итоговые матрицы; нужно доказать отсутствие наблюдения промежуточных состояний. |
| B, GL cache | Exact uniform cache, immutable FBO pool | Не реализован | Потенциальные повторные записи есть; нет полного driver call trace и безопасной модели всех invalidations. |
| B, Present | Clear перед full drawable blit | Не реализован | В штатном случае clear перезаписан; ещё не доказаны scissor/resize/error paths, сохраняющий чёрный fallback. |
| B, experiments | Раздельный display-rate vote; loading-only redraw limit | Не реализован | Меняют видимую cadence/latency; аппаратный эксперимент и отдельная приёмка обязательны. |
| C, scheduling | Completion-aware parking и единый SDL event snapshot | Не реализован | Требуют корректных notify/predicate/memory ordering и input/lifecycle модели. Защищённый scheduler не менялся. |
| C, GPU passes | Tile discard, объединение/пропуск проходов | Не реализован | Нет доказательства последнего consumer и аппаратной стоимости tile stores. Качество и liveness нельзя угадывать по elapsed scope. |
| C, correctness | Необнулённые удалённые GL handles при reset | Отдельная отложенная проверка | `CRT::destroy`/`CTexture::Unload` и ранние create/load guards требуют reset/reuse fixture. Это риск корректности, а не доказанная экономия. |

### A: нулевая скорость слушателя OpenAL

Изменён только [SoundRender_CoreA.cpp](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/xrSound/SoundRender_CoreA.cpp:149), `CSoundRender_CoreA::update_listener`: удалён повторный `alListener3f(AL_VELOCITY, 0, 0, 0)`. `_initialize` по-прежнему задаёт нулевую скорость после создания и выбора каждого контекста. Позиция, ориентация, master gain, environmental interpolation и commit остаются прежними. `_restart` применяет окружение, `_clear` уничтожает контекст; новый контекст снова проходит инициализацию.

До правки полный поиск по `src` находил ровно два writer этой velocity: инициализацию и повтор в update. После неё остался только initializer. Реальный OpenAL Soft `al/listener.cpp::alListener3f` берёт `mPropLock` и для равного значения не возвращается раньше: вызывает `CommitAndUpdateProps`. При немедленной публикации `al/state.cpp::UpdateContextProps` копирует свойства и атомарно заменяет pending update; контейнер берётся из freelist либо выделяется при её пустоте. Устранена реальная публикация свойств, а не только присваивание трёх float. Сколько фактических allocations исчезнет после прогрева, офлайн не измерено.

Fixture извлекает исходную функцию из before-копии и фактическую изменённую функцию из production; отдельно проверяет неизменность инициализации. **49 152 update / 12 contexts:** 147 456 → 98 304 вызова listener API, итоговые состояния слушателя и все остальные вызовы совпадают. Мутации ненулевой начальной velocity и velocity, зависящей от позиции, корректно ломают проверку. ASan/UBSan с leak checking проходят; обе мутации под sanitizers дают ожидаемый assertion/exit 2. OpenAL API здесь заменён записью вызовов; сам mixer и аудиосигнал не эмулируются.

Raw byte comparison: удалена ровно одна 54-байтовая CRLF-строка, остальные байты сохранены. Before SHA256 `52c9faf2fd839d1fd730b9755eccb7a9b91aacf3762513d708789c2346ba38f3`, after `c88fb2dcffffe551e53f71d7eb4ed1bf18e4b62c1290fd0ac02068fbdfb56355`. Actual ARM64 Android24 NDK27 Release translation unit без устаревшего PCH скомпилирован в объект 229 820 байт, diagnostics отсутствуют. [Patch](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/cpu-lifecycle/listener-a1.patch), [итог проверок](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/cpu-lifecycle/listener-implementation.json).

### A: неиспользуемый jitter texgen point light

Изменён только [r3_rendertarget_accum_point.cpp](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build/src/Layers/xrRender_R2/r3_rendertarget_accum_point.cpp:88), `CRenderTarget::accum_point`: удалены локальный `m_Texgen_J` и его вычисление через `u_compute_texgen_jitter`. Полученная матрица нигде не читалась. Используемый screen texgen, uniforms, shaders, culling, stencil и draw calls не менялись. В `accum_spot` похожая матрица используется и сохранена.

Helpers в `r2_rendertarget.cpp::u_compute_texgen_jitter` и реальные `Fmatrix::mul/identity` изменяют только свои destination. Удалены два полных matrix multiply, identity и временные stores/copies на вызов `accum_point`. Это CPU-работа внутри области local lights; GPU draws, pixels и проходы не сокращаются. Общий `draw_calls` нельзя выдавать за число point lights.

**Работа присутствовала в старом ARM64 release binary, компилятор её не устранил.** В renderer из обычной `cordonperf1` SHA256 `7ad6b76175bd41de29223709dafd3f566766331bdfb7c4d8cc52ae5c56a8fb04` функция начинается с `0x3400a0`: нужный screen multiply находится около `0x340884`, затем оставались jitter multiply `0x3408bc`, identity `0x3408cc` и второй multiply `0x340904`. Это внешние вызовы xrMiscMath, сохранившиеся при ThinLTO. [Исходный disassembly](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/gpu-memory/accum-point-jitter-arm64.txt) доказывает наличие работы в контроле, а не сам по себе её удаление из новой APK.

**8192 differential cases** извлекают реальные accum_point/texgen/matrix bodies; покрыты MSAA, fp16 blend, stencil, shadow/fullsize/translucent варианты, разные transforms и размеры. Записанные state/render/uniform команды побайтово совпадают, число multiply меняется 3 → 1. Две мутации обнаруживают возврат dead jitter и порчу используемого screen texgen. ASan/UBSan проходят. Android ARM64 Release TU проходит `-fsyntax-only` с GPU diagnostics OFF и ON, normal preprocess не содержит GLGpuProfiler calls.

Перед изменением файл сверен с root source-baseline.zip; после удаления ровно двух строк все прочие байты, включая CRLF, сохранены. Before SHA256 `a2e42c2191295e0b60d94ee49214f4691caf36136972737d2db0f423bfa8adf6`, after `52a5713cddc70235e76b9e035a60321011b00b8ee2d0895e58baf40939d560c3`. [Patch](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/gpu-memory/A1-unused-jitter.patch), [host-проверки](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/gpu-memory/A1-host-results.json), [NDK-проверки](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/gpu-memory/ndk-results.json).

### Общая сборка и граница binary proof

Root собрал обычную `outputs/openxray-soc-thermala1-arm64.apk`, версия `1.6.02-thermala1`, **33 697 516 байт**, SHA256 `e54eccbedfa879cc3c9efedf4c492c31f50de84b8a759fd33c1f9f195c02b4dc`.

[APK verifier](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/outputs/angle-work/thermala1-apk-report.json) подтверждает 29 ARM64-библиотек, совместимость ELF с 16K pages, подпись v2, прежний сертификат и C++ runtime, наличие ANGLE, отсутствие прямых GL imports у renderer и неизменность входных исходников при сборке. Снимок входов: `outputs/angle-work/thermala1-inputs.json`. Обычный build не включает standalone pacer или новые B-hooks.

Точное отделение доказательств:

| Проверка | Статус на момент подготовки отчёта |
|---|---|
| Старый jitter реально исполнялся в release binary | Подтверждено ARM64 disassembly `cordonperf1` |
| Новый OpenAL update source; zero initializer сохранён | Подтверждено извлечёнными production-функциями, raw bytes и NDK object compilation |
| Новый jitter source; используемые render/state данные сохранены | Подтверждено извлечёнными production-функциями, raw bytes и NDK checks |
| Общая сборка normal `thermala1` и APK verifier | PASS |
| Final APK disassembly OpenAL/jitter | **PASS**, фактические APK сравнены по точным function ranges, аргументам и PLT/GOT/relocations; см. proof ниже |
| Native/ANGLE runtime, длительная стабильность и энергия | Контрольные A/B/C получены и world/no-console подтверждены root. Кандидат установлен, Native load пройден; candidate SF/ANGLE и полная приёмка не завершены из-за отключения устройства. Энергия неизвестна |

[Final binary proof](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/cpu-lifecycle/final-binary-proof/README.md) сравнивает библиотеки, извлечённые прямо из `cordonperf1` и `thermala1` с проверенными APK-хэшами. У `CSoundRender_CoreA::update_listener` export `0x4d198`, size 196 → 176: исчезли ровно пять ARM64-инструкций нулевых `d0/d1/d2`, `w0=AL_VELOCITY(0x1006)` и соответствующий BL. Вся оставшаяся функция совпадает после нормализации адресов PLT и локальных переходов. Position/orientation сохранены; velocity initializer BL `0x4ccd0` присутствует в обеих APK. Точный update связан с vtable через relocation slot `+0xb8`; это не сравнение случайных alias symbols.

В exact `accum_point` export `0x3400a0` прежние прямые `Fmatrix::mul` BL `0x340884/0x3408bc/0x340904` стали единственным `0x340848`; identity BL `0x3408cc` исчез. Целевые функции проверены через PLT → GOT → `R_AARCH64_JUMP_SLOT`. Standalone jitter helper остаётся в библиотеке для других мест: ThinLTO встроил соответствующее тело в accum_point, поэтому глобальное отсутствие helper symbol не было критерием. Размер accum_point вырос 13 440 → 13 468 байт из-за другого codegen; этот размер не выдаётся за скорость.

Candidate ELF Build IDs: `libxrSound.so = d208c0e65d297085bfe87f02e7da75401c68704a`, `libxrRender_GL.so = 8eacc36379ddac95e07fad0107606d976e106f3c`. `libopenal.so` побайтово одинаков в обеих APK. [Машинная проверка](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/cpu-lifecycle/final-binary-proof/proof.json) имеет PASS; manifests сохраняют ELF hashes/Build IDs и подтверждают сохранность production sources. Binary proof не измеряет runtime timing, реальные allocations или энергию.

## Почему в APK нет нового ограничения FPS

Путь кадра: `CApplication::Run` → SDL pump/lifecycle/window events → `CRenderDevice::ProcessFrame` → `FrameMove/seqFrame` → camera/parallel tasks → `DoRender` → `CHW::Present` → task wait → старый limiter. Тот же loop обновляет input, уровень, scripts и другие frame-driven объекты. Ограничение этого loop может уменьшить число таких обновлений в секунду в быстрых сценах. Сохранение самих callback-функций не делает limiter независимым от игровой логики.

Физика накапливает dt и использует fixed substeps, поэтому меньше render frames не обязательно означает меньше physics steps. Однако smoothing/clamp dt, max substeps и cadence потребителей остаются существенными. Scheduler также dispatches часть работы на каждый Update, часть — по игровому времени и budget. Обещать прежнюю частоту всей логики без отдельного исследования нельзя.

`StandaloneFramePacer60.h` находится только в `D/pacing` и требует явный evidence compile guard. Он использует `steady_clock`, рациональные периоды 16 666 666 / 16 666 667 ns, один blocking `sleep_until`, не крутится в spin/yield, сбрасывает расписание при overrun/gap/reset и не делает catch-up bursts. 3600 виртуальных быстрых кадров дают 60 секунд; 25 ms workload не получает лишний sleep. Проверены oversleep, pause/gap, early wake, faulty clock и ещё 100 000 виртуальных кадров. Host, WSL ASan/UBSan и NDK syntax checks проходят. **Прототип не подключён к production и не входит в `thermala1`.** Его тесты доказывают арифметику и state machine, а не реальные FPS, latency или экономию энергии.

Текущие настройки и код важно читать раздельно:

- `-unlock_fps` отключает обычный gameplay limiter, запрашивает swap interval 0 и включает Java vote за максимальную частоту среди display modes с теми же физическими размерами. Эти запросы не гарантируют effective cadence.
- Без unlock команда `rs_fps_limit 60` задаёт integer период `1000/60 = 16 ms`, то есть арифметически 62,5 FPS при мгновенной работе и идеальном сне. Это не точное расписание 60 Hz. Меню имеет отдельную ветку; loading draw проходит ещё до неё.
- Старый limiter измеряет кадр через паузимый/масштабируемый `TimerGlobal`. В полностью paused кадре его длительность равна нулю и sleep добавляется поверх работы. Исправление часов может ускорить меню и увеличить его расход энергии; это B.
- Исторический восстановленный `user.ltx` предыдущей приёмки имел gameplay/menu limits 501, VSync off. Это сохранённый исторический файл, не сегодняшнее чтение настроек устройства.
- Interval 1 при панели 120 Hz не означает cap 60. SDL 2.30.12 положительный interval 2 передаёт выбранному EGL driver; при success getter хранит запрошенное 2. Driver может иметь иной effective диапазон/policy, чего этот cache не измеряет. Fake-driver fixture специально показывает getter=2 при effective=1. Реальный interval 2 и panel 120 Hz ещё нужно подтвердить; при panel 60 Hz такой интервал номинально соответствовал бы 30 Hz.

Нельзя включать pacer или swap interval 2 как «render-only thermal fix», обходя запрет менять cadence логики. При тяжёлом кадре дольше 16,67 ms cap60 может не создать никакого idle времени. Меньше FPS, больше sleep или запрос меньшей display rate сами по себе не доказывают оптимизацию при прежнем результате.

## Lifecycle, потоки и CPU-работа: что найдено и что уже нормально

| Путь | Подтверждённое поведение | Решение |
|---|---|---|
| SDL/background | Default Android SDL pump после передачи DIDENTERBACKGROUND блокируется на ResumeSem, сохраняет и восстанавливает EGL context. Engine drain/guard нужен для этого порядка. | `continue` в engine не объявляется background spin по одному фрагменту кода; существующий lifecycle fix сохранён. |
| Idle Task Workers | `TaskWorkerStart` при отсутствии работы блокируется в `Event::Wait` → `pthread_cond_wait`. | Число созданных threads не равно числу постоянно занятых cores. Произвольное уменьшение worker count не предложено. |
| `TaskManager::Wait` | Пока task не закончена, главный/ожидающий worker помогает выполнять и красть работу; когда её нет, попытки продолжаются. | Потенциальный CPU расход требует thread-CPU trace. Completion parking — C, scheduler не переписывался. |
| Asset preparation | `GameDataInitThread` выполняет подготовку однократно и завершает работу. | Startup FS activity не выдаётся за per-frame overhead. |
| WakeLock и экран | В manifest есть разрешение WAKE_LOCK; явного Java acquire не найдено. GameActivity использует обычный FLAG_KEEP_SCREEN_ON. | Разрешение не доказывает удержание CPU. Фактические runtime wake locks проверяет root. |
| Overlay | Нет постоянного Choreographer redraw loop. Предыдущий camera-only MOVE fix уже уменьшает invalidation. | Повторять прошлую оптимизацию нельзя; cache метрик label оставлен низкоприоритетным C. |
| Logcat capture | Ожидает `readLine`; не крутится в idle. На каждую пришедшую строку делает file flush. | B — отдельное batching-решение с сохранением error/close/rebind contracts, не остановка capture наугад. |
| Native log | Android `AddOne` flushes каждую строку независимо от ForceFlushLog. | Удаление `-force_flushlog` не исправит этот путь. Частота логов и реальная стоимость не измерены. |
| `ft` diagnostics | Guard только Android/non-editor; после первых verbose frames продолжаются atomic/mutex/snprintf checkpoints. Detached watchdog просыпается раз в секунду до смерти процесса, background guard подавляет probes, но не таймер. | B — изоляция диагностики. Существующая корректная background/heartbeat защита должна сохраниться. |
| Device accelerometer | `SDLSurface` регистрирует SENSOR_DELAY_GAME, каждый sample проверяет rotation и вызывает JNI onNativeAccel. Engine открывает game controllers и использует controller gyro; generic accelerometer consumer не найден. | B — сначала отделить orientation, учесть custom mapping и сохранить физический controller gyro. Простое выключение регистрации небезопасно. |
| SDL controller scan | Native detect раз в 3 секунды вызывает Java scan устройств. | Реальная, но низкочастотная повторная работа; event-listener redesign только после измерения, C. |

Точные точки: `SDLActivity::handleNativeState/onPause/onStop`, `SDLSurface::handlePause/handleResume/onSensorChanged`, `AndroidAppLifecycle::State::Drain`, `CApplication::Run`, `CRenderDevice::OnWindowActivate`, `TaskManager::TaskWorkerStart/Wait`, `AppLog::startLogcatCapture/writeRaw`, `device.cpp::ft::note/watchdog_main/install`.

Большинство локальных bind-transform vectors, найденных в IKFoot/IKLimb, CarWeapon, Helicopter и WeaponStatMgun, создаются при setup/spawn. Их наличие не доказывает allocation/frame. Multiplayer-only Actor auto-pickup не является hotspot single-player Кордона; sound environment ID vector относится к загрузке. Task allocator уже использует thread-local fixed storage. Ни глобальный allocator, ни protected skeleton/task код не менялись.

## GPU, uploads, память и незавершённые кандидаты

Повторный разбор старого `frozen4-native-fhd-210023-352362/gpu-profile.jsonl` подтвердил 1375 последовательных CPU rows, 43 GPU rows, 1920×1080, один World/frame и среднее 4708,659 backend draws/frame. Luminance вызывается один раз на кадр. В том историческом окне elapsed render total составлял 17,404 ms, geometry 5,321 ms, sun 4,474 ms, local lights 4,196 ms, combine 2,480 ms; luminance — 0,069 ms. Это **исторические пересекающиеся scopes**, не сегодняшняя производительность, hardware utilization, bandwidth или energy. Их нельзя складывать или целиком считать доступной экономией. Неполный JSONL tail сохранён как invalid, а не нулевая выборка.

**Luminance vertices, B.** `CRenderTarget::phase_luminance`, `r2_rendertarget_phase_luminance.cpp`: три quad каждый кадр генерируют одинаковые `4×48 + 4×144 + 4×144 = 1344` байта. При текущем stream upload три Unlock ведут к трём glBufferSubData ranges. 8192 извлечённых вариантов функции и ASan/UBSan доказывают постоянство bytes и draw order. После прогрева staging memory уже переиспользуется; allocation/frame здесь не доказана. Dedicated immutable VBO потребует constructor/destructor/reset, QuadIB rebind, корректных base vertex/offset и запрета на stale IDs. Экономия всех 0,069 ms luminance не следует из этого факта.

**Промежуточные transforms, B.** `accum_point/accum_spot/accum_reflected` используют последовательные `set_xform_world/view/project`; текущие setters рассчитывают семь products для итоговых WV/VP/WVP и могут несколько раз перезаписать uniforms до draw. Combined setter должен пройти fixture настоящих R_xforms, ctable remap, invW validity и наблюдений на каждом draw; качество/precision и порядок видимых данных должны сохраниться.

**Uniform cache, B.** В `glr_constants_cache.h::R_constants::set/seta` равные payload могут снова уйти в GL. До реализации нужны attempted/changed diagnostic counters. Key должен включать program generation/location/type/count, данные сравниваются точно, invalidation покрывает relink/delete/reuse/context, raw writes и array overlap. Общий cache не равен уже принятому grass batching; его нельзя добавлять без этой модели.

**Present clear, B.** `CHW::Present` clear default draw framebuffer затем делает color blit на всю drawable. При успешном full coverage clear выглядит лишним, но ошибка blit сейчас оставляет чёрный fallback; scissor, actual drawable, resize/orientation и error paths ещё не доказаны. Исторические 0,308 ms present scope не являются стоимостью одного clear. Final blit/filter/разрешение сохраняются.

**FBO и tile memory.** Physical-state, draw-buffers и texture-binding caches уже действуют; повторно предлагать их как отсутствующие нельзя. Immutable FBO pool — B с ownership/invalidation ценой. `glInvalidateFramebuffer` требует доказать последнего consumer depth/stencil/RT; текущие геометрия, light masks, forward и shadows читают эти ресурсы. Преждевременный discard, удаление теневых passes или объединение shading passes — C.

**Отдельный reset/reuse риск, C.** `glSH_RT.cpp::CRT::destroy` удаляет `pRT`, не обнуляя scalar, при том create имеет `if(pRT)return`; `ResourceManager_Reset.cpp` вызывает reset_begin/end surviving CRT. `CTexture::Unload` также не обнуляет `pSurface`, а Load имеет ранний guard. DeleteTextures принимает const pointer и сам caller handle не обнуляет. Destruction Target может маскировать проблему; настоящий context loss пока не проверен. Это требует отдельного source-extracted reset/reuse теста перед добавлением persistent caches, а не несогласованного исправления в thermal APK.

Статические текстуры уже защищены bLoaded и кешем binds; per-frame повторной загрузки всех текстур нет. Theora использует thread-local CPU buffer и upload только нового video frame. DXT decode относится к загрузке. Sampler creation относится к state compilation, а mip bias Apply уже имеет dirty check. Их нельзя называть доказанными sustained hotspots выбранной сцены. Переиспользование sun culling vectors требует проверки reentrancy/worker ownership и не уменьшает shadow pixels само по себе.

## Что проверено на компьютере

| Проверка | Итог | Предел доказательства |
|---|---|---|
| OpenAL production differential | 49 152 updates, 12 contexts, states/other calls equal; 2 mutations detected | Mock API; без реального mixer и звукового сигнала |
| OpenAL ASan/UBSan + leaks | PASS, stderr пуст; mutations дают только expected assertion | Корректность host fixture, не Android/audio timing |
| OpenAL actual NDK27 ARM64 TU | PASS, Release Android24 object | Один TU, не отдельная полная APK-приёмка |
| Point-light production differential | 8192 cases, render/state/uniform bytes equal, mul 3 → 1; 2 mutations detected | Recording stubs, без GL driver |
| Point-light ASan/UBSan и NDK OFF/ON | PASS | NDK syntax-only; actual APK binary proof отдельно PASS, runtime кандидата дополняет root |
| Luminance invariant fixture | 8192 cases, ASan/UBSan PASS | Доказана постоянность bytes; новый lifetime VBO не реализован |
| Текущий limiter/VSync/Present | 14 limiter + 5 VSync cases, реальные timer classes/Wait, 3 Present traces; sanitizers PASS | Подтверждает модель существующего кода, не экономию |
| Standalone pacer60 | Виртуальные 60 s, overrun/oversleep/gap/fault cases, 100 000 stress frames; host/NDK/sanitizers PASS | Не подключён, не Android cadence и не разрешение на gameplay tradeoff |
| SDL interval2 fixture | Передача 2 в EGL, requested cache, fake clamp/reject/negative paths PASS | Реальные Native/ANGLE support и present cadence неизвестны |
| Новый power/thermal collector | **36 host tests PASS**, default --plan без ADB | Parser/guard/read-only contract; hardware monitor root проверяет отдельно |
| Combined normal thermala1 build/verifier | PASS | Runtime/энергия остаются отдельными |

Fixtures сохраняют извлечённые source fragments и hashes, чтобы не проверять вручную пересказанный алгоритм. MinGW и WSL sanitizer-сборки доступны; это не замена driver/context/device tests. Источники отдельных результатов перечислены в трёх исходных audits; [collector offline-proof](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/telemetry/offline-proof.json) фиксирует 36 тестов и отсутствие ADB на офлайн-этапе.

## Power/thermal collector и честная интерпретация

[power_samples.py](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/telemetry/power_samples.py) по умолчанию показывает план; только `--run` читает уже работающий процесс. Период не меньше 5 секунд, нет burst-догоняния после медленных команд. Output создаётся эксклюзивно. Сохраняются сырые dumpsys/sysfs/proc блоки, их источники, command timing, PID/starttime и признаки missing/denied/error. SF parser переиспользуется из предыдущего проверенного collector, алгоритм не копируется. [Единицы и ограничения](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/telemetry/partial_units_overview.md).

`Thermal Status` — Android enum, не градусы. Vendor temperature/current/voltage/frequency nodes сохраняются raw с `unit=unknown`, пока единицы и направления не подтверждены. Отказ доступа не превращается в ноль. `getconf CLK_TCK` читается отдельно; process/thread ticks и clock brackets нельзя выдавать за GPU utilization или полную CPU-загрузку устройства. KGSL gpubusy tuple не объявляется средним процентом всей серии.

При thermal status ≥ LIGHT, неизвестном/override status, PID exit/reuse или повреждённом monitoring collector завершает сбор с явной причиной. Это **остановка telemetry**, не автоматическое закрытие игры: пауза/остановка и экран off принадлежат root. Collector не запускает и не останавливает игру, не меняет settings, не делает install/input/batterystats reset.

Поля `device_power_w` и `energy_j` остаются null. При USB charging ток батареи, даже с верными единицами, не равен полной мощности SoC/дисплея или устройства. Температура зависит от исходного состояния, окружающей среды и тепловой инерции. Thermal status 0 не доказывает равенство мощности. Небольшое изменение градусов в коротких окнах нельзя приписывать патчу.

Нужны отдельно maximum-throughput и matched-cadence сравнения. При снятом cap сокращение CPU-работы может разрешить больше кадров с прежним или большим расходом энергии. Host call counts, FPS, GPU elapsed scopes, thread CPU, wait time и SF actual-present — разные величины. Сохранение картинки и cadence при меньшей работе даёт основание для дальнейшего измерения, но не само измерение joules.

Прошлый результат grass batching около +12% Native FHD относится к контрольной паре 22 октября; P95/P99 тогда почти не изменились. Эта цифра не относится к двум новым A-правкам и не является baseline энергопотребления.

## Аппаратный этап: полученный контроль и оставшаяся проверка кандидата

Получены три контрольные серии **старой `cordonperf1`**, каждая по два 30-секундных SF-окна. Capture metadata сообщает модель `SM-X710`. Реальный FBO **1920×1080**, drawable 2560×1600 подтверждены engine log каждой серии. Native renderer — Adreno 740; C использует фактический ANGLE Vulkan renderer. `r2_aa off`, VSync off, menu cap 60; оба запрошенных игровых фактора после load — 0.001. Сейв `cg22benchp4`; координаты, предоставленные root: `[-55.414333, -2.253043, -64.284225, 0.928497, -0.231284, 0.290519]`.

Root подтвердил world без консоли после launch и на game-after для A и B. Для C также просмотрен game-after около 15:43 MSK: правильная дорожная сцена, консоль отсутствует. [root-verification.json](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/root-verification.json) сохраняет эти отдельные визуальные подтверждения. Значение console_closed для C равно true; это не доказывает равенство погоды или энергии. Одинаковый save и факторы 0.001 не выдаются за самостоятельное доказательство равенства абсолютного времени окружения/погоды; этот отдельный verification остаётся неизвестным.

| Серия | Реальный режим | FPS первого / второго окна | FPS по сумме интервалов и длительности | Визуальная проверка |
|---|---|---:|---:|---|
| A, `control-native-a-fhd-150518-617589` | Native, `-unlock_fps`, config501 | 46.082 / 46.023 | **46.053** | Root: launch и after world без консоли |
| B, `control-native-b-fhd-151738-316785` | Native, без unlock, существующий config60 | 46.126 / 45.786 | **45.956** | Root: launch и after world без консоли |
| C, `control-angle-c-fhd-152952-066113` | ANGLE, без unlock, существующий config60 | 41.718 / 39.734 | **40.726** | Root: launch и after world без консоли |

| Окно | Median / P95 / P99 present interval, ms | 1% low, FPS |
|---|---|---:|
| A1 | 25.000 / 33.333 / 33.333 | 30.000 |
| A2 | 25.000 / 25.000 / 33.333 | 30.000 |
| B1 | 25.000 / 33.333 / 33.333 | 30.000 |
| B2 | 25.000 / 33.333 / 33.333 | 30.000 |
| C1 | 25.000 / 33.333 / 33.333 | 28.364 |
| C2 | 25.000 / 33.333 / 33.333 | 25.714 |

Все шесть SF distributions непрерывны, process/layer и config в окнах стабильны, интервалов >100 ms в этих коротких окнах нет. Это не утверждение об отсутствии редких фризов. Weighted FPS считается по сумме interval count / сумме duration; медианы, P95/P99 и 1% low не усредняются между окнами. 1% low — обратное среднего худших ceil(1% N) интервалов, а не обратный P99. Полные значения и ссылки на capture folders — [analysisresults-control-abc.json](C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/thermal-power-20261023/analysisresults-control-abc.json).

**Тяжёлая сцена даёт около 46 FPS и с Native uncapped, и с существующим config60.** Средний present interval около 21.7–21.8 ms уже больше периода 16–16.67 ms. Эти окна не демонстрируют, что cap60 создаёт полезное idle время, снижает мощность или охлаждает устройство. A/B меняют unlock/cap policy, а не бинарный A-патч. C имеет иной backend и более тёплый старт; его FPS не служит строгим выводом об эффективности API.

| Серия | Первый readiness, ms | Максимальный guard sample, ms | AP raw до SF → последний полностью внутри SF | Whole-process CPU context delta |
|---|---:|---:|---|---|
| A | 125 | 125 | 27.7 → 48.0 | 7643 user + 1019 system ticks, CLK_TCK=100; 86.62 CPU s |
| B | 109 | 141 | 28.4 → 48.0 | 7728 user + 1016 system ticks, CLK_TCK=100; 87.44 CPU s |
| C | 93 | 140 | 35.4 → 47.8 | 8660 user + 307 system ticks, CLK_TCK=100; 89.67 CPU s |

В этих сериях in-window guard выполнял только thermalservice и pidof; full context snapshots сняты до/после SF и отделены host monotonic bounds. Initial readiness прошёл 200 ms gate. Android thermal status в guard samples — 0 без override; USB powered=true, другие charging connection flags=false. Значения AP остаются raw с неизвестными единицами. Terminal guard AP для C — 50.1 уже после SF; оно не подставляется вместо последнего полностью in-window 47.8. Штатный sampler exit2 после intentional force-stop считается ожидаемым только при завершённом SF, cleanup marker и подтверждении, что PID stop находится после SF end; это не crash посреди capture.

CPU s в таблице вычислены только из накопленных `/proc/PID/stat` ticks и отдельно прочитанного CLK_TCK, с неизменными PID/starttime. Это сумма CPU всех threads между внешними context snapshots: она включает работу вне точных SF-границ, не равна render CPU time и не является измерением мощности. Частоты и vendor counters сохранены как endpoint snapshots с исходными units/availability, без вывода utilization. Исходный AP различается; физический thermal/power выигрыш из этой таблицы не следует. W/J остаются null.

Отдельно исключены:

- `control-native-uncapped-141937-672886`: полный диагностический observer занимал около 9–10 секунд на sample и читал множество недоступных узлов; VALIDITY_NOTICE запрещает использовать его FPS/нагрев как контроль.
- `control-native-uncapped-valid-144047-753568`: readiness 953 ms превысил gate; SF не запускался, FPS отсутствует.
- `control-native-a-144957-847914`: launch фактически имел FBO1728×1080, без SF-замеров. Launcher cached graphics signature оказался несовместим с повторным push исходного user.ltx; root сделал явный vid_mode1920 в тестовой копии и actual FBO assert. Эта попытка не объявляется FHD; таблицы выше используют только новые подтверждённые FHD captures.

`analyze_device_runs.py` и его 13 локальных проверок сохраняют эти исключения, проверяют expected cleanup, техническую валидность и отдельную строгость пар. В текущей сводке нет допущенных patch A/B сравнений: все три серии сняты одним старым APK, отличаются policy/backend/стартом, а weather/absolute clock equality не приписывается автоматически. Root дополняет кандидата отдельно.

### Кандидат установлен; устройство отключено до сравнительного замера

В 15:44 MSK выполнен `install -r` кандидата **thermala1**. Readback SHA256 установленного APK совпал с `e54eccbedfa879cc3c9efedf4c492c31f50de84b8a759fd33c1f9f195c02b4dc`; UID 10361 сохранён, тестовый user.ltx и все 12 файлов savedgames до/после установки побайтно совпали по SHA256. Evidence: `D/install-thermala1-154431/result.json`. Первая попытка installer завершилась до install из-за имени поля Android16 `appId` вместо `userId`; helper исправлен для обоих форматов. Это не вылет игры и не потеря данных.

`D/device-runs/candidate-native-d-fhd-154511-255525` подтверждает фактический Native Adreno740, FBO1920×1080, menu render, load `cg22benchp4`, оба time factors0.001. Root просмотрел `loaded-before-esc.png`: правильная сцена, интерфейс и консоль закрыта. Это smoke загрузки/картинки, не проверка качества звука или всех point lights. Статический кадр не доказывает отсутствие редких визуальных дефектов.

Начало candidate measure в 15:47 MSK (`candidate-native-d-fhd-154725-677461`) остановилось на первом `thermalservice`: ADB device not found. **Ни одного SF-окна кандидата нет.** Candidate ANGLE, измеренный FPS выигрыш, долгий thermal тест, audio, point lights/shadows и полноценный HOME/Recents smoke остаются невыполненными.

**Восстановление пользовательских настроек ещё pending.** До disconnect тестовый конфиг имел FHD, cap60/menu60 и time_factor_single0.001; исходные сегодняшние настройки — 1728×1080,501/501,time_factor_single10. Последний успешный launch погасил экран и перевёл SDL в background pause. После потери USB финальные PID/экран и force-stop подтвердить невозможно; состояние не выдаётся за проверенное.

Первое действие при следующем подключении — `python work/thermal-power-20261023/restore_today.py --run` из W. Helper reviewed, `--plan` не использует ADB, host input hashes PASS; **на устройстве ещё не выполнен**. Он возвращает сегодняшний exact11673-byte user.ltx (SHA256 b129ad651af57f7191734fe91dc98a80d9a1e85931e978a826bccbf94dd568b8), все28 UI states из первого snapshot, проверяет12 original saved files и завершает force-stop/screenOFF. SharedPreferences сохраняются через краткий menu-only StartGame, без load/save, затем launcher переоткрывается для persisted UI проверки. Root не делал uninstall, сброс данных или глобальные настройки.

Текущая сборка — **кандидат с доказанным удалением лишней работы**, а не подтверждённый thermal/performance релиз. Большие категории B/C в неё не включены. Продолжение описано в `NEXT_THERMAL_OPTIMIZATION_SESSION.md`, полный статус — `D/session-state.json`.


Сравнение должно удерживать одинаковые scene/camera, фактические размеры, качество, эффекты, погоду/часы и сопоставимое исходное thermal состояние. Одинаковый SAV сам по себе не фиксирует время окружения; `time_factor_single` и `al_time_factor` ранее расходились после load. Сегодняшние значения записывает root, любые тестовые конфиги сохраняются и затем восстанавливаются. Замеры Native и ANGLE не смешиваются. Повторные A/B/A/B и длительный прогон важнее одного короткого окна; изменения display vote/cap не должны скрытно становиться второй осью эксперимента.

Для возврата из HOME использовать существующую задачу через Recents с проверкой Activity/PID. Известный ложный crash marker после искусственного повторного MAIN launcher над живой остановленной GameActivity не является доказательством native crash. Launcher ради этого аудита не менялся.

Дальнейшая работа после аппаратной приёмки A — выбирать один B-кандидат по измеренной стоимости и проверяемому контракту. Больше неиспользуемых caches, меньше threads или более низкий cap сами по себе не являются результатом оптимизации. В текущей APK завершены только два описанных устранения лишней CPU-работы; тепло и энергия требуют отдельного подтверждения.
