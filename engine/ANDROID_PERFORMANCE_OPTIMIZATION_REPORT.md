# Автономная оптимизация OpenXRay Android — 21–22 октября 2026

## Статус и границы доказательств

**Готов отдельный offline-кандидат `1.6.02-perf20261022`: девять изменений в 13 production-файлах. Сборка, линковка, Release lintVital, подпись, ELF, frozen inputs и сохранность игровых ресурсов проверены. На планшет кандидат НЕ установлен и в игре НЕ запускался.**

APK: `W/outputs/openxray-soc-perf20261022-arm64.apk`, 33 696 780 байт.
SHA256: `7e12dac3fb52cc081ee3dc49e49641e58acc03725def2cbd2a01fc03e55fa6f3`.
Пакет: `org.openxray.dev`; подпись совместима с прежними тестовыми сборками.
Сборка завершилась успешно за 2m42s; это длительность сборки на ПК, не игрового теста.
Сохранены 29 ARM64-библиотек, 16KB alignment, прежний C++ shared runtime и оба графических пути.
Проверены hashes 10 543 source files, включённых в freeze; прежние FS/SkeletonRigid/GameTask fixes побайтно сохранены.

Все **2197 негенерируемых игровых/графических assets** совпали с angleexposure2. Всего APK содержит 2199 assets: из них изменился только создаваемый AGP `assets/dexopt/baseline.prof` — Android ART startup compilation hints для обновлённого DEX. Это не шейдер/настройка/графический ресурс. Первый чрезмерно общий asset-equality assert остановил проверку на этом профиле; различие исследовано, разрешены только два известных generated ART-profile пути, полный game/render comparison затем прошёл. Скрипт не исключает произвольные assets из сравнения. Библиотеки ANGLE и `libc++_shared.so` совпали побайтно. Из native libraries изменились только `libxrCore`, `libxrEngine`, `libxrGame`, `libxrRender_GL`, `libxrSound`.

Механизм отката дополнительно испытан в изолированных workspace fixtures: dry-run, точный CRLF/LF restore, backup текущих файлов, повторный no-op, отказ при поздних пользовательских правках/выходе пути/неверном архиве. На реальном репозитории выполнен только dry-run отката; оптимизации остаются в исходниках.

Работа относится к существующим **Native OpenGL ES** и **OpenGL ES → ANGLE → Vulkan**. Нативный Vulkan-рендер не разрабатывался. Планшет в этой сессии недоступен: ADB, установка, запуск игры и изменение настроек устройства не выполнялись. Измерений FPS, GPU time, температуры и игровых frame times нет. Результаты настольных тестов ниже не являются результатами Tab S9.

Главный результат — устранение доказанной лишней работы и нескольких дефектов с проверяемыми локальными контрактами. Не предполагается, что эти правки гарантируют большой прирост FPS: основное ограничение игры пока не установлено. Изменения не снижают разрешение, качество, дальность, освещение, тени, AA, геометрию или эффекты.

### Обозначения

- **Реализовано и проверено offline**: исходники изменены, указанные проверки действительно выполнены.
- **Измерено на ПК**: число операций/вызовов или время конкретного host-теста, без переноса результата на Android.
- **Гипотеза**: ожидаемый эффект по исходникам, который ещё нужно измерить на устройстве.
- **Отложено**: не входит в production-кандидат. Проверка Android-компиляции сама по себе не доказывает одинаковую картинку.

## Исходное состояние и откат

Рабочая область `W = C:/Users/User/Documents/Codex/2026-10-11/gjvyb`.
Репозиторий `R = W/work/openxray-build`.

Перед изменениями сохранены **фактические рабочие исходники с уже существовавшими незакоммиченными исправлениями**, а не только Git HEAD:

- `W/work/perf-20261021/source-baseline.zip`: 11 461 файл, включая исходники 11 инициализированных submodules и необходимые игнорируемые JNI-библиотеки; 51 871 243 байта.
- SHA256 архива: `39831b2f8b24b0c785be452fa33769d48cfd64da9116d0df70c51342e40c65c4`.
- `baseline-manifest.json`: SHA256 каждого файла, Git HEAD основного дерева и submodules, состав снимка. Архив и все записанные элементы проверены.
- `baseline-working-tree.diff`, `git-status-before.txt`, `user-task.txt`.

Снимок не включает signing keys, SDK, toolchain, build caches и историю `.git`. Он предназначен для отката исходников в существующем окружении. Папка названа датой начала работы 21 октября; часть проверок завершена уже 22 октября.

Git HEAD до работы: `28e6ff87203f0b93bf1935c9692b282d605b7072`, ветка `main`. Коммиты/push/оформление релиза отложены по прежнему указанию пользователя; снимок служит разрешённым альтернативным механизмом отката. `git reset --hard`, `git clean` и перезапись чужих изменений не применялись.

Предыдущая проверенная APK сохранена отдельно и не изменяется:

`W/outputs/openxray-soc-angleexposure2-arm64.apk`

SHA256 `d31ef8e016b42f4d65efd81334f477fc6c098c018d210f9b3de58ae5b7826abe`, 33 694 964 байта, `org.openxray.dev 1.6.02-angleexposure2`.

Для безопасного отката только правок этой сессии предусмотрен `W/work/perf-20261021/restore_optimization.py`. По умолчанию это dry-run. С `--apply` он сначала проверяет архив, все пути и исходные/итоговые hashes, отказывается перезаписывать последующие изменения, сохраняет текущие изменяемые файлы отдельным ZIP, затем восстанавливает baseline bytes. Новые документы, Git, APK и данные планшета он не удаляет и не меняет.

## Что исследовано

### Реальный графический путь

1. `android/.../RenderBackendConfig.java`: выбор и проверка пары `libEGL_angle.so`/`libGLESv2_angle.so`, API 29 для ANGLE; Native поддерживает API 24. SDL получает абсолютные пути через `SDL_VIDEO_EGL_DRIVER`/`SDL_VIDEO_GL_DRIVER`; `ANGLE_DEFAULT_PLATFORM=vulkan` задаётся до создания контекста. Выбор фиксируется на процесс, при смене требуется перезапуск процесса.
2. `LauncherActivity.java`, `OpenXRayActivity.java`: применение выбранного пути при запуске и сохранение настроек. Новых параметров качества, разрешения или профилей в этой работе не добавлялось.
3. `src/Layers/xrRenderGL/glHW.cpp`, `CHW::{CreateDevice,Reset,UpdateViews,Present,DestroyDevice}`: создание контекста SDL, FBO, вывод через blit и swap, сброс кэшей. Сохраняется очистка backbuffer для безопасного letterboxing.
4. `src/Layers/xrRender_R2/r2_R_render.cpp`, `CRender::Render`; `xrRenderPC_GL/gl_rendertarget_phase_combine.cpp`: общий R2/deferred-путь, геометрия, накопление освещения, combine и AA. Порядок проходов не меняется.
5. `glR_Backend_Runtime.h`, `glBackendStateCache.h`, `glTextureBindings.h`: состояние фактического GL-контекста разделяется между CPU command lists. Подмена такого кэша локальным состоянием одного списка ранее приводила к повреждению изображения; этот принцип сохранён.
6. `glStreamSync.h`, `glBufferUtils.cpp`, `src/Layers/xrRender/r__sync_point.cpp`: существующий CPU upload/BufferSubData и обход глобального fence при безопасном upload-пути. Это прежняя оптимизация, её нельзя записывать в новый выигрыш.
7. `glr_constants_cache.h`, `glr_constants.cpp`, `glSH_*.cpp`, `xrRenderPC_GL/rgl_shaders.cpp`: uniform uploads, GLES monolithic programs, существующий shader cache и его условия.

Предыдущий `angleexposure2` действительно показывал освещённый загруженный SoC-сейв на Native и реальном ANGLE/Vulkan на Tab S9. Это исторические smoke tests, а не запуск нового кандидата. Одна проверка ухода/возврата с погашенным экраном прошла. Полная потеря EGL-контекста и длительный игровой сеанс этими проверками не покрыты.

ANGLE pinned revision: `e5640f4ec9ce5fec357186fd7fa40ad3030f2698`.
Локальный источник: `/home/user/oxr-angle-linux-e5640f4ec9ce/client/angle` в Ubuntu-24.04.
Изучены реальные `libANGLE/Context.cpp`, `Framebuffer.cpp`, `BlobCache.cpp` и Vulkan `DisplayVk.cpp`. ANGLE уже имеет собственные state caches и Vulkan resource/pipeline machinery. В частности, повтор viewport отсекается на его стороне; просто продублировать все кэши без разбора — слабое обоснование оптимизации. Для `drawBuffers` найдена безусловная работа с dirty state/type masks даже при повторе того же списка.

Драйверные `.so` ANGLE не пересобираются и не изменяются этой работой. Не применялись скрытые feature overrides, отключение validation или драйверные hacks. Общая GLAD-таблица движка и ImGui сохранена; прямые GLES/EGL imports в renderer не возвращаются.

### Синхронизация и блокирующие операции

- `CHK_GL` уже не вызывает `glGetError` для каждого объекта в текущем Release. Это прежнее решение, не новая оптимизация. Комментарий в `xrDebug_macros.h`, сравнивающий любой `glGetError` с `glFinish`, нельзя считать измерением или универсальным фактом о драйвере.
- В нормальном графическом пути поиска не обнаружены безусловные покадровые `glFinish`/`glFlush`, которые можно просто удалить. `glClientWaitSync` есть в старом fence-пути, уже обходимом действующим `-gles_stream_upload`.
- В upload остаются ограниченные startup/error checks; они не превращены в бесконечную покадровую диагностику. Checks ошибок выделения/потери ресурсов не вырезались.
- `glReadPixels` в screenshots/shadow dumps и opt-in ANGLE exposure audit нужен для диагностики. Audit выключен в нормальном профиле. Его stall нельзя использовать как доказательство тормозов обычного рендера.
- `CHW::Present` сохраняет clear/blit/swap и letterboxing. Убирать clear без доказательства покрытия, scissor/masks и корректности рамок рискованно.

### CPU, Android, ввод, ресурсы и микрофризы

Проверены JNI/Java touch flow, cache методов/JNIEnv, SDL controller ownership, lifecycle Surface, блокировки журналов, sound source loading и I/O callbacks, UI callbacks, scheduler/task waiting и mutable collections.

В старом capture `native-baseline-20261019-220113/logcat-app.txt` были 80 подробных UI-строк и 179 одинаковых сообщений `DetailPathManager`. Это исторические event bursts/предупреждения; они не доказывают покадровый bottleneck текущего кандидата. Старый loading watchdog попал в поиск звуков через `CLocatorAPI::exist`; такой sample не доказывает, что каждый текущий вызов делает `stat`, и не оправдывает общий negative cache.

Потенциальная просадка после разбивания ящиков пока не локализована: возможны физика/создание обломков, звуковые cache misses, аллокации, visibility/draw/shader work. Без нового профиля нельзя назначить виновником Vulkan, physics или конкретный mutex. Геймплей и физика в этой работе не менялись.

## Реализованные изменения

Подробные before/after patches, исходные hashes и воспроизводимые fixtures находятся в `W/work/perf-20261021/tests/`. Итоговый список production-файлов — `optimization-changes.json`; совокупный patch относительно сохранённого пользовательского состояния — `optimization-vs-baseline.patch`.

### 1. Быстрый пропуск уже очищенных texture stages

**Файл:** `src/Layers/xrRenderGL/glTextureBindings.h`, `GLTextureBindings::{detail::Cache,Bind,ClearStage}`.

Раньше повторное очищение уже пустого texture stage снова обходило шесть поддерживаемых targets. Сами GL binds старый кэш уже отсекал, но проверки оставались. Добавлен per-stage признак доказанно пустого состояния. После reset состояние неизвестно и проходит полный старый обход; любой реально выполненный bind инвалидирует доказательство. Force и stage вне кэша идут старым путём; desktop-ветка не изменена.

**Сохранено:** точная последовательность GL-вызовов, active texture unit, supported-target logic, sampler behaviour, deletion и внешний reset. Никаких новых выделений; 48 булевых признаков на контекст.

**Проверки:** baseline/candidate — 2 377 980 операций, 60 профилей ES3.0/3.1/3.2/extensions/limits; 1 957 390 одинаковых GL trace events, 219 092 попадания в быстрый путь. Мутант без bind invalidation отвергается тестом. Полный Android NDK syntax check `glSH_Texture.cpp`, ASan/UBSan/leak detection пройдены.

**Host microbenchmark, 9 повторений:** warm clear median 17.5075 → 13.159 ns/stage при нуле GL calls в обеих версиях; смешанный 48-stage/2–13-used сценарий 782.40 → 583.56 ns/pass при одинаковых 31 236 mock GL calls на 50 000 passes. Это стоимость конкретного синтетического host-кода, не оценка экономии игрового кадра или FPS.

Evidence: `tests/texture_clear/{README.md,results.json,android-syntax-results.json,sanitizer-results.json,mutation-results.json}`.

### 2. Точный кэш draw-buffer selection

**Реализовано и проверено offline; входит в APK.** По 450 096 операций actual-header + actual `CBackend::set_FB` для GLES, Android и desktop. На GLES/Android вызовы selection сократились с 231 819 до 220 619; физическое состояние, 82 636 invalid calls и ошибки сохранены. Desktop: 231 819 → 231 819. Это синтетический корпус, не число вызовов игрового кадра.

Missing-bind-invalidation mutant обнаружен. ASan/UBSan/leak detection и пять полных ARM64 translation units без устаревшего PCH проходят. Независимый реальный EGL/GLES software test на существующем Mesa llvmpipe ES3.2: 20 checkpoints с полным сравнением pixel arrays/MRT state, шесть GL error cases, clear/draw/blit, private diagnostic FBO restore и reuse имени FBO; всё совпало. Фактические drawBuffers calls в этом fixture 53 → 40. Это не ANGLE/Vulkan/Adreno runtime и не GPU benchmark.

Evidence: `tests/draw_buffers/{README.md,source-audit.json,results.json,android-syntax-results.json,sanitizer-results.json,mutation-results.json,software/results.json}`.

Граница доказательства: как существующий framebuffer cache, helper предполагает корректный upstream bind известного FBO. Не добавлены glGetError к каждому bind или FBO registry. Произвольные будущие external GL writers обязаны сбрасывать физический кэш. Полная потеря контекста не считается проверенной этим fixture.

Область: `glBackendStateCache.h`, `glR_Backend_Runtime.h`, `glHW.cpp`, `glSH_RT.cpp`, `glSpotShadowDump.h`, `xrRenderPC_GL/gl_rendertarget_u_set_rt.cpp`.

Цель — убрать повторную отправку совершенно одинаковых `glDrawBuffers` для текущего известного FBO. Key содержит точное число и порядок attachments, включая `GL_NONE`; переход между двумя и тремя элементами не приравнивается. Состояние отдельное от attachment cache. При FBO bind, reset, внешнем изменении или неизвестном framebuffer доказательство сбрасывается; force обязан отправлять вызов. Неподдержанные/невалидные параметры передаются исходному API. Порядок проходов и содержимое attachments не меняются.

Возможный эффект — меньше GLES entry/ANGLE validation/dirty bookkeeping. Экономия CPU на Adreno и влияние на Vulkan render passes ещё неизвестны. Проверки всех прямых bind/draw mutations и программного GLES отражаются в итоговом evidence; применять непроверенный вариант запрещено.

### 3. Устранены лишние invalidation экранных кнопок

**Файл:** `android/.../TouchOverlayView.java`, `onTouchEvent`, `ACTION_MOVE`.

Camera-only MOVE меняет картинку SDL, но не вид overlay. Теперь `invalidate()` запрашивается при изменении joystick thumb или видимого pressed-state кнопки. Передача ввода/JNI, порядок событий, относительное движение, haptics и release неизменны.

**Проверки:** actual Java class baseline/candidate, 1 620 input/config/layout операций; native/haptic/raw-float/Canvas traces совпали; все 572 визуальные перемены вызывают redraw. В сценарии 1 502 → 582 invalidation requests (920 лишних запросов устранены). Это запросы, а не 920 реально отрисованных Android кадров. Реальное планирование UI и пиксели на планшете требуют проверки. Android34 javac пройден.

### 4. Потокобезопасное форматирование Java timestamp

**Файл:** `android/.../AppLog.java`, `formatTimestamp`, три call sites.

Один `SimpleDateFormat` использовали UI/setup/logcat threads. Добавлена узкая отдельная блокировка только вокруг format; она отпускается до main logging lock и sinks. Формат/Locale, момент создания Date, содержимое строк, flush и error handling не меняются.

**Проверки:** воспроизведены одновременные входы baseline; candidate — 8 threads × 12 000 суммарных записей без overlap, все 12 000 записей/flush сохранены. Fixed-clock normal/throwable output совпадает, сценарий обратного порядка блокировок проходит, полный Android34 javac пройден. Это исправление гонки, не обещание ускорения.

### 5. Освобождение SDL controller handle

**Файл:** `src/xrEngine/xr_input.cpp`, обработчик `SDL_CONTROLLERDEVICEREMOVED`.

Owned handle закрывается через `SDL_GameControllerClose` перед удалением из `controllers`. Неизвестный или принадлежащий внешнему владельцу handle не закрывается. Baseline воспроизводил пять ownership failures из девяти сценариев; candidate проходит 9/9. Полный NDK syntax check пройден. Реальные Bluetooth/USB hotplug и ввод на планшете не тестировались. Изменение устраняет утечку ссылки, не меняет чувствительность или управление.

### 6. Убран дублирующий stderr mirror Android Release

**Файл:** `src/xrCore/log.cpp`, `AddOne`, `CreateLog`, `CloseLog`.

Каждая строка уже направляется в logcat, память, callback и файл, но Linux `OutputDebugString` дополнительно использует `std::cerr` с двумя unit-buffered insertions. В Android Release после CreateLog этот дубликат выключен по умолчанию. Точный standalone `-stderr_log` его возвращает. Android DEBUG, desktop и ранние фазы инициализации сохраняют зеркало; CloseLog сбрасывает выбор для следующего engine lifecycle.

**Не ослаблены crash diagnostics:** logcat, LogFile, callback, файл и покстрочный Android file flush сохранены. Удаление file flush и буферизация журнала не применялись. Начальные fixture tests: 3 build modes × 16 argument scenarios с одинаковыми primary sinks/flush. Независимая проверка дополнительно выявила reinit phase с уже non-null Params до второго CreateLog. Старый вариант воспроизвёл ошибку; после reset в CloseLog финальный logger повторно прошёл эти ASan/UBSan scenarios и affected NDK object compile. Полный active suite проверен уже на окончательных исходниках.

### 7. Подробные UI callback traces стали opt-in

**Файл:** `src/xrGame/ui/UIScriptWnd.cpp`, `CUIDialogWndEx::{SendMessage,AddCallback}`.

Обычные focus/event bursts раньше форматировали и синхронно записывали подробные `>>>` строки. Теперь они включаются точным `-ui_callback_trace`. Реальные errors других подсистем сохраняются. Выбор первого callback, pointer/name matching, регистрация, invocation и inherited fallback не изменены.

12 scenario fixtures с извлечённым реальным `event_comparer` и production bodies подтверждают те же вызовы/fallback. Флаг читается из текущих Params, без навсегда устаревающего раннего static. Это сокращение event logging, не доказанный постоянный bottleneck.

### 8. Убран повторный поиск существующего звука

**Файл:** `src/xrSound/SoundRender_Source.cpp`, `CSoundRender_Source::load`.

Один результат `FS.exist(fn)` используется для диагностики и загрузки. `_EDITOR` после перехода к `$no_sound.ogg` обязательно выполняет новый lookup. Общего negative cache нет. Предположение локальной эквивалентности — indexed asset set не меняется внутри этого прямолинейного вызова; обычные игровые файлы этому соответствуют.

8 комбинаций MASTER_GOLD/_EDITOR/Windows × 16 случаев сохраняют результат, путь, diagnostics и decode attempts. В обычном non-MASTER_GOLD success path убран один indexed lookup; это не обязательно системный `stat`.

### 9. Безопасная целочисленная граница чтения Vorbis

**Файл:** `src/xrSound/SoundRender_Source.cpp`, `g_ov_callbacks.read`.

Количество целых блоков рассчитывается integer division с ограничением оставшимися байтами; нулевые size/nmemb и неположительный remaining возвращают 0. Старый float мог округлить 16 777 219 remaining bytes до 16 777 220 при size=1 и запросить лишний байт. Multiplication нового пути ограничено remaining.

100 000 old/new обычных случаев и 200 640 независимых wide-integer oracle/boundary/random случаев пройдены с ASan/UBSan. Нулевые guards, отрицательный remaining, порог float precision, INTPTR_MAX/SIZE_MAX покрыты. Это I/O correctness fix, не FPS-оптимизация. Отдельная обработка отрицательного `OV_HOLE` в decompress не менялась.

### Общие проверки CPU/Android

До независимого review выполнены восемь полных NDK компиляций baseline/candidate для четырёх CPU/resource translation units. Portable fixtures используют извлечённые production bodies; OS/engine services вокруг них заменены явными mocks. Они не запускают весь движок, APK или реальный libvorbis decoder.

Java/input evidence: `tests/android_input/{README.md,results.json}`.
CPU evidence: `tests/cpu_resources/{CPU_RESOURCE_FINDINGS.md,active-results.json,ndk-results.json,independent-review.md,logging-followup-results.json,ndk-logging-followup-results.json}`. Полный current active suite — `verify_active.py`; он проходит на окончательных исходниках, проверяет baseline SourceManager и не выдаёт экспериментальную ownership-проверку за часть кандидата. Старый `verify.py` с прямым запуском и `tsan_check.py` сохраняют исторический эксперимент; после исключения SourceManager patch нельзя использовать их default entry point как приёмку active production. Исторические результаты sound owner experiment ниже нельзя приписывать финальному APK.

## Исключено после проверки и отложено

### Sound source publication experiment — исключён из кандидата

В `i_create_source` load выполняется вне `s_sources_lock`; два cache misses могут загрузить один id, второй `emplace` проваливается и возвращается некэшированный owner. Локальная повторная проверка перед heap allocation исправляет ownership в модели. 40 forced races × 16 threads воспроизвели baseline дефект, исправленный вариант проходил ASan/UBSan/TSan.

Но независимый review установил: реальный `CSoundRender_Source` содержит `shared_str`, у которого `_set/_dec` меняют plain `dwReference` без atomic; fixture использовал `std::string`. Освобождение проигравшего временного объекта добавляет путь decrement, и локальный TSan не доказывает безопасность этого реального пути. Baseline тоже не доказывает thread safety concurrent loads, однако расширять риск в основной сборке нельзя.

**`src/xrSound/SoundRender_Core_SourceManager.cpp` восстановлен побайтно из baseline.** `tests/cpu_resources/03-source-cache-owner.patch` и результаты сохранены как исследование. Для продолжения нужны actual shared_str lifetime/race fixture, подтверждение реальных потоков загрузки и протокол владения/загрузки. Нельзя просто сериализовать тяжёлый decode общим mutex без оценки stalls.

### Другие кандидаты, не включённые в APK

| Идея | Почему отложена / что нужно доказать |
|---|---|
| Skip повторных uniform uploads по memcmp | Связь program generation/relink, shared command lists, external writers, array ranges и reuse ids. Текущий uniform batching уже есть; потеря invalidation возвращает геометрические артефакты. Нужна полная lifetime модель и driver tests. |
| Persistent monolithic shader/pipeline cache | `rgl_shaders.cpp` существующий binaryCacheEnabled требует separate shader objects; GLES monolithic путь не получает автоматически полноценный engine disk cache. ANGLE BlobCache/Vulkan cache уже есть, но engine/SDL registration persistent callbacks не найдена. Нельзя объявлять всё cache отсутствующим: возможен vendor/system cache. Нужны cold/warm capture, driver/backend key, corruption fallback, лимиты, atomic writes и lifetime callbacks. |
| Удалить Present clear / изменить invalidateFramebuffer | Не доказаны полное покрытие backbuffer, scissor/color mask/letterbox и lifetime attachment содержимого. Смена tile load/store semantics требует pixel tests и Adreno профиля. |
| Persistent mapping/orphan/unsynchronized streams | В истории уже были растянутые модели и хаос текстур. Действующий upload-путь не заменять без строгого in-flight resource lifetime proof. |
| Меньше SDL event pumps | Влияет на pause/resume handshake и задержку управления. Нужен Android event trace, а не только статический подсчёт. |
| Удалить snapshots в CObjectList/CUIWindow | Snapshot защищает обход при callbacks, меняющих контейнер. Удаление может дать invalid iterator/use-after-free. |
| AI/pathfinding retries, scheduler/TaskManager backoff | Меняют поведение/планирование; исторические warnings не дают оснований менять game AI ради FPS. |
| Асинхронная загрузка/общий FS negative cache | Нужен правильный invalidation, порядок загрузки и сохранение ошибок. Sound collection допускает пропуски suffix: прерывать поиск на первом missing нельзя. |
| Изменить per-line file flush/watchdog tracing | Уменьшает надёжность crash tail/диагностики, а текущая стоимость не измерена. Сохранены. |
| Кэш размеров label/перерисовка Surface/lifecycle redesign | UI/font/config changes и Surface ownership требуют device tests; текущий исправленный lifecycle сохранён. |
| Форсировать ANGLE feature flags, memory modes или отключить barriers | Версия/драйвер/нагрузка определяют корректность и стоимость. Без GPU capture риск высокий. |

Не включались пониженные graphics presets, render scale, динамическое разрешение, отключение sun/shadows, LOD/visibility reductions, урезание particles/AI/physics, изменение shader maths, brightness или AA.

## Что означает Full HD именно здесь

| Размер | Пикселей | Относительно 1920×1080 |
|---|---:|---:|
| 1728×1080 — прежний 16:10 1080p тест Tab S9 | 1 866 240 | 0.90 |
| 1920×1080 — требуемый FHD 16:9 сценарий | 2 073 600 | 1.00 |
| 2560×1600 — нативный экран Tab S9 | 4 096 000 | 1.9753 |

Это арифметика pixel count, **не прогноз FPS**. FHD имеет на 11.11% больше пикселей, чем прежние 1728×1080; это разные условия. Новых размеров/scale в конфиг устройства не записывалось. В актуальном launcher 1080p может сохранять aspect ratio экрана; при будущем тесте надо записать фактический render width/height из лога. Нельзя назвать 1728×1080 тестом 1920×1080. Для 16:9 на 16:10 нужен корректный aspect/letterbox, без искажения кадра.

В `src/Layers/xrRender_R2/r2_rendertarget.cpp` позиции/accumulator используют `D3DFMT_A16B16G16R16F`, generic buffers — `A8R8G8B8` с ветками по caps/options; shadow surfaces/depth создаются по smapsize. Сырые 8 bytes/pixel дают около 15.82 MiB на один RGBA16F FHD target, 4 bytes/pixel — 7.91 MiB на RGBA8 target, без alignment/compression/driver overhead. Это оценка одного target, не измерение общей GPU памяти; конкретные ветки/caps и временные Vulkan representations нужно смотреть на устройстве. Смена форматов ради экономии без quality proof не применялась.

**Растёт с пикселями:** G-buffer, полноэкранные combine/FXAA/bloom/luminance, lighting volumes по покрытой площади, backbuffer blit, memory bandwidth RT/depth. Shadow maps зависят прежде всего от собственных размеров и количества света/casters, а не линейно от разрешения экрана. Без замеров неизвестно, сколько этих проходов ограничено ALU, texture sampling, bandwidth или synchronization.

**Растёт с draw calls/passes:** CPU submission, uniforms, state checks, render target selection, часть ANGLE validation/pipeline lookup. Новые state оптимизации направлены сюда и не обязаны давать больший эффект при Full HD.

**Растёт с объектами/взаимодействиями:** visibility, AI/physics, transforms, sounds, allocations, новые shader/material combinations. Это возможный источник spikes после ящиков; диагноза ещё нет.

Новый кандидат не уменьшает pixel shader work или число отрисованных объектов искусственным способом. Если GPU time окажется доминирующим, большие CPU improvements могут почти не изменить FPS; это нормальный возможный результат.

## Проверки сборки и артефактов

Окружение сохранено: JDK `W/work/toolchain/jdk-17.0.20.1+1`, Android SDK `W/work/android-sdk`, compileSdk34, NDK `27.0.12077973`, CMake `3.22.1`, Gradle cache `W/work/gradle-cache`. ARM64 Release/C++17, действующий ThinLTO/16KB alignment не менялись. Signing key остаётся в `W/work/android-user/debug.keystore` и не включён в снимок исходников/отчёт.

Команды из PowerShell, working directory W:

```powershell
$py = 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
& $py .\work\perf-20261021\candidate_pipeline.py prepare --variant perf20261022
& .\work\build.ps1 -Variant perf20261022 *> .\work\build-perf20261022.log
& $py .\work\verify_angleaudit_apk.py --variant perf20261022 --expect-angle present
& $py .\work\perf-20261021\candidate_pipeline.py verify --variant perf20261022
& $py .\work\perf-20261021\restore_optimization.py
```

Prepare отказывается перезаписывать frozen inputs. После исходного prepare повторять его без причины нельзя; frozen source inventory предназначен для конкретной сборки. Build запускает только assembleRelease; install не входит. Verify сравнивает подпись с предыдущими сборками, ARM64/16KB ELF/dependencies/shared CRT/GLAD dispatch, отсутствие embedded GitHub token и неизменность исходников. Дополнительная проверка сравнивает game/render assets с angleexposure2, отдельно учитывает только два известных generated ART startup profile paths, сверяет ANGLE pair/CRT, сохраняет отдельную APK и не заменяет baseline.

Повтор active CPU regression suite в WSL:

```powershell
wsl.exe -d Ubuntu-24.04 --exec python3 /mnt/c/Users/User/Documents/Codex/2026-10-11/gjvyb/work/perf-20261021/tests/cpu_resources/verify_active.py
```

Команды texture/draw/Android fixtures и их реальные NDK flags записаны в README и JSON соответствующих каталогов. Детерминированные mocks и mutation tests являются проверкой локальных контрактов, не заменяют Android rendering.

Если последующие изменения требуют другого build, использовать новый уникальный variant и собственный freeze. Полный `lintRelease` в исходной базе уже имел 32 errors/149 warnings; это не новый чистый lint run. Нормальные Release lintVital checks выполняются сборкой. Обновлять зависимости/глобальные настройки ради скрытия старых lint проблем не требовалось.

Итоговые отчёты: `W/outputs/angle-work/perf20261022-{inputs,apk-report,regression-report}.json`. Успешная сборка и совпадение assets не доказывают pixel equality на Adreno — state bugs могут менять итог даже при тех же шейдерах.

## Первые проверки, когда пользователь вернётся

1. Backup установленного APK, `user.ltx`, launcher/touch prefs доступным способом и saves; сверка signature/UID. Только update install, без uninstall. Этот этап ещё не выполнялся.
2. Candidate → Native GLES, прежний загруженный сейв, одинаковые параметры/камера/погода. Проверить солнце/фонарик/костёр, тени NPC/объектов, оружие, листву, UI/AA/тонмап, инвентарь/торговлю/ПДА/квесты/сохранения, звук и несколько циклов background/resume.
3. Перезапуск процесса → ANGLE Vulkan. Подтвердить requested/effective/live GL_RENDERER; тот же сейв/условия/проверки. Два backend должны остаться доступны. Controller hotplug проверить при наличии геймпада, обычный touch — всегда.
4. Повторить обе проверки на baseline APK при любом новом дефекте; менять одну переменную. При сомнении использовать известную angleexposure2, а не включать старые опасные flags.
5. Только затем performance A/B. Отдельно baseline Native vs candidate Native и baseline ANGLE vs candidate ANGLE; сравнение разных API в разных условиях ничего не говорит о конкретной оптимизации.

### Воспроизводимый performance протокол

- **Сцены:** фиксированный уличный обзор Кордона с листвой/солнцем; костёр/несколько NPC и фонарик в тёмном помещении; один scripted маршрут; отдельный повторяемый тест разбивания одного типа ящика из одинакового save. Главное меню не является игровым тестом.
- **Неизменные условия:** фактическое разрешение (отдельно1920×1080), render scale, preset/AA/texture/shadow/view distance, sun/time/weather, camera FOV/position/direction, FPS cap/unlock/refresh rate, touch overlay и фоновые приложения. Нормальный профиль, без audit/readbacks/forced rebind.
- **Прогрев:** отдельно cold start после нового cache и несколько warm runs; cache не удалять между вариантами без записи причины. Стабилизировать температуру; записывать battery/charging/thermal status и частоты. ABBA порядок и несколько повторов уменьшают влияние нагрева.
- **Frame times:** median/p90/p95/p99 и распределение spikes, длительность sample/пропуски/паузы. FPS average недостаточно. Loading, idle/menu и traversal помечать отдельно.
- **CPU:** Perfetto/simpleperf при доступности — main/render, Java UI, logging/file I/O, resource loading, physics, scheduler/task waits, JNI; время до submission, kernel waits, contention/allocations. Sampling overhead тоже учитывать.
- **GPU:** AGI/доступные Adreno counters и asynchronous timestamp queries при подтверждённой поддержке. Чтение query result сразу после выдачи запрещено в чистом performance run: оно само может ждать GPU. Для disjoint timer queries проверять disjoint и отбрасывать невалидные samples.
- **ANGLE:** shader/pipeline cache misses, компиляция/link/program creation, render pass breaks, CPU validation/translation, vkQueue submit/present waits, allocation/copy counts при доступных инструментах. Число draw/state calls и cache hit rates полезнее догадок по одному FPS.
- **Surface/frame pacing:** BufferQueue/SurfaceFlinger/app frame timelines, blocked swap/present, actual vs requested refresh. UI FPS counter и частота дисплея не равны времени GPU кадра.
- **Память:** PSS/native/Java heap, GPU allocations при доступности, peak при level load/reload и повторениях ящиков; проверка, что разовая экономия не куплена постоянной утечкой.

Если GPU tools/профилирование недоступны, начать с одинаковых frametime samples + CPU trace и честно оставить GPU attribution неизвестной. Программный GLES — дополнительная проверка корректности, не производительности Adreno. Необходимые данные пока отсутствуют: новые device traces, фактические CPU/GPU times, per-pass timing, число calls/frame, thermal/clock history, реальные memory/cache counters.

## Запуск и диагностика

Нормальный профиль сохраняет прежние автоматически добавляемые аргументы: `-xclsx -gles_shadow_detach -gles_stream_upload -gles_uniform_batch`. Пользовательское поле дополнительных аргументов не требуется заполнять новыми optimization flags.

`-stderr_log` возвращает только stderr mirror; `-ui_callback_trace` — подробную UI callback диагностику. Они не нужны для обычной игры или чистого A/B.

`-gles_angle_audit`, shadow dumps, `-gles_state_rebind` — диагностические режимы, не performance baseline. `-gles_highp_position` оставлен opt-in как прежняя диагностика; он не исправлял black world сам по себе. Отказавшиеся эксперименты `-gles_stream_orphan`/`-gles_stream_sync` не включать. Native GLES сохранён как рабочий альтернативный путь.

Особо защищены прежние исправления FS mapping/SkeletonRigid/GameTask и POINT/CLAMP/no-MIP чтение R32F exposure history. Возвращать LINEAR для 1×1 истории нельзя: ANGLE не объявлял `GL_OES_texture_float_linear`, что ранее делало мир почти чёрным. Все девять Lua tonemap contracts и параметры AA сохраняются.

## Вывод

На уровне исходников и воспроизводимых локальных тестов устранена лишняя работа state/UI/logging и несколько ограниченных дефектов ресурсов/I/O. Изменения рассчитаны на оба GLES-пути; это не специальное магическое ускорение Vulkan. Большие архитектурные решения оставлены за пределами основной сборки, а один формально проходивший тесты concurrency fix исключён после независимого review.

Следующий решающий шаг — одинаковая device-проверка картинки/управления/звука и A/B CPU/GPU/frame-time измерения нового кандидата против сохранённой angleexposure2. До неё честный ответ о приросте FPS: **не измерен и не гарантирован**.
