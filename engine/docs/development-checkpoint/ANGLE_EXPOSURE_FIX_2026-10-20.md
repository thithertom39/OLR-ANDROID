# ANGLE: исправление экспозиции и проверка Native GLES / Vulkan — 20 октября 2026

## Актуальный checkpoint

Состояние зафиксировано по отчёту от **20.10.2026 16:15 MSK**. Чёрный мир в проверенном сохранении SoC исправлен. Финальная сборка **`org.openxray.dev 1.6.02-angleexposure2`** собрана, проверена, установлена на Tab S9 и прошла ограниченную проверку одной загруженной сцены на **Native OpenGL ES** и **существующем GLES через ANGLE → Vulkan**. Проверено также одно выключение/включение экрана с возвратом картинки в том же процессе ANGLE. Это не нативный Vulkan-рендер и не полная приёмка игры.

Этот документ и финальный runtime summary имеют приоритет над историческими записями «ANGLE ещё не собран», «ждём загрузки angleaudit», «причина неизвестна», «работа приостановлена до завтра» в старых разделах handoff. Старые отчёты оставлены неизменными как доказательства соответствующих этапов, а не текущего состояния.

- Windows workspace `W`: `C:/Users/User/Documents/Codex/2026-10-11/gjvyb`.
- Корень исходников `R`: `W/work/openxray-build`.
- Проверенное устройство: Samsung Tab S9 **SM-X710**, Adreno 740; текущая проверка относится к этому устройству.
- Финальный APK: `W/outputs/openxray-soc-angleexposure2-arm64.apk`, **33 694 964 байта**.
- SHA256 APK: **`d31ef8e016b42f4d65efd81334f477fc6c098c018d210f9b3de58ae5b7826abe`**.
- Авторитетный итог: `W/outputs/angle-work/angleexposure2-runtime-report.json`; переносимая копия — [angleexposure2-runtime-report.json](../../evidence/angle-integration/angleexposure2-runtime-report.json).
- Общая архитектура, Linux recipe, загрузка библиотек и ранее выполненная интеграция — [ANGLE_INTEGRATION_2026-10-19.md](ANGLE_INTEGRATION_2026-10-19.md). Её ранние результаты рассматривай как историю.

Пользователь разрешил root-агенту самостоятельно управлять подключённым планшетом для этих проверок, включая загрузку последнего сохранения. После проверки игра **закрыта штатно**, подтверждён `OpenXRayActivity.onDestroy`, четыре файла сохранений не изменились, экран переведён в сон. Полного выключения устройства не было. Пользователь отсутствует; никакой продолжающийся игровой тест этим checkpoint не подразумевается.

## Установленная причина чёрного мира

Путь ANGLE изначально действительно создавал Vulkan-контекст и загружал игру. Наличие чёрного мира не было ошибкой пункта лаунчера или случайным возвратом к системному GLES. Живые renderer/provider logs подтверждали ANGLE/Vulkan, а HUD и отдельные эффекты были видны.

Причиной проверенного затемнения оказался **некорректный sampler для истории экспозиции**:

1. `src/Layers/xrRender_R2/r2_rendertarget.cpp`, TONEMAP-блок, создаёт `rt_LUM_pool[]` как **1×1 `D3DFMT_R32F`**. В GL это `GL_R32F`; соответствие находится в `src/Layers/xrRenderGL/glTextureUtils.cpp`.
2. `src/Layers/xrRender_R2/r2_types.h` определяет `r2_RT_luminance_src = "$user$tonemap_src"` и `r2_RT_luminance_cur = "$user$tonemap"`.
3. `src/Layers/xrRenderPC_GL/gl_rendertarget_phase_combine.cpp::CRenderTarget::phase_combine()` связывает эти alias с двумя элементами exposure pool, затем меняет их местами после кадра.
4. В `src/Layers/xrRender/blenders/blender_luminance.cpp::CBlender_luminance::Compile()`, **`R_GL`, `case 2`**, `s_tonemap` ранее использовал `r_Sampler_clf`: CLAMP, LINEAR min/mag, без mipmaps.
5. Реальный ANGLE/Vulkan-контекст **не объявляет `GL_OES_texture_float_linear`**. При этом `EXT_color_buffer_float`, `EXT_color_buffer_half_float` и `EXT_float_blend` объявлены. Renderability/blending не доказывают возможность линейного чтения R32F.
6. В исходниках закреплённого ANGLE `src/libANGLE/formatutils.cpp`, запись `GL_R32F`, filter support требует `Extensions::textureFloatLinearOES`. `src/libANGLE/Texture.cpp::TextureState::computeSamplerCompleteness()` допускает point filtering, а для линейного фильтра проверяет поддержку формата. Поэтому этот sampler/texture pair неполон, хотя сам framebuffer корректен.
7. `res/soc/gamedata/shaders/gl/bloom_luminance_3.ps` читает прошлую экспозицию из `s_tonemap`, вычисляет новый scale и делает `lerp(scale_prev, scale, MiddleGray.w)`. Потеря корректного feedback нарушала адаптацию, искажающаяся экспозиция затем влияла на картинку мира/неба.

Не описывай это как «в текстуре всегда записан ноль» или «FBO не работает»: диагностические чтения дали **конечные положительные** числа. Все 16 проверок до/после восьми кадров показали complete FBO с ожидаемым собственным R32F texture, без MSAA; реальная разрешённая пара readback — `GL_RED/GL_FLOAT`. До исправления поздние измерения экспозиции составляли примерно **0.09348 / 0.10168**. Нулевой fallback неполной выборки прошлого значения согласуется с этой картиной, но непосредственный результат shader sample отдельно не читался.

Причинность подтверждена отдельной сборкой **angleexposure1**: изменён только фильтр `s_tonemap` в финальной luminance-редукции, сохранены тот же ANGLE backend, сохранение, конфиг и диагностические флаги. Мир, небо, поверхность, здания, NPC и оружие стали отчётливо видны; поздние значения экспозиции стали **1.51389825 / 1.51396966**. Это проверка единственного изменения, а не вывод из одновременной смены нескольких настроек. См. [angleaudit-runtime-report.json](../../evidence/angle-integration/angleaudit-runtime-report.json) и [angleexposure1-runtime-report.json](../../evidence/angle-integration/angleexposure1-runtime-report.json).

## Минимальная правка и завершение контракта ресурсов

### C++: история экспозиции

В `src/Layers/xrRender/blenders/blender_luminance.cpp`, `R_GL case 2`, под `#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)` используется:

```cpp
C.r_Sampler_rtf("s_tonemap", r2_RT_luminance_src);
```

В desktop GL остаётся прежний `r_Sampler_clf`. R2/DX-ветви не менялись. `s_image` для 8×8 RGBA16F редукции по-прежнему линейный — его менять нельзя вместе с feedback sampler, иначе меняется алгоритм усреднения.

Реальные helper-функции в `src/Layers/xrRender/Blender_Recorder_R2.cpp` определяют:

- `r_Sampler_rtf`: CLAMP, POINT min, NONE mip, POINT mag;
- `r_Sampler_clf`: CLAMP, LINEAR min, NONE mip, LINEAR mag.

Для единственного текселя 1×1 и конечных координат при CLAMP оба фильтра дают то же значение, если LINEAR поддержан. POINT устраняет требование неподдержанной возможности, не снижая детализацию изображения. Не менялись формат R32F, тональная кривая, шейдер, освещение, G-buffer или разрешение.

SHA256 исправленного `blender_luminance.cpp`: `5fbc8fd703c05dbdda0275e5dd0adcee063b6031d61674bf63fd1f8a835434a1`.

### Lua: остальные чтения 1×1 `$user$tonemap`

В финальную angleexposure2 включены ровно девять однострочных изменений:

```text
res/{soc,cs,cop}/gamedata/shaders/gl/clouds.s
res/{soc,cs,cop}/gamedata/shaders/gl/combine_volumetric.s
res/{soc,cs,cop}/gamedata/shaders/gl/portal.s
```

Только цепочка `sampler("s_tonemap"):texture("$user$tonemap")` получила `:clamp():f_none()`. Проходы, blend/depth/fog/sorting и фильтры остальных текстур сохранены. В `src/Layers/xrRender/ResourceManager_Scripting.cpp` эти методы действительно зарегистрированы: `_clamp()` вызывает `i_Address(..., D3DTADDRESS_CLAMP)`, `_f_none()` — `i_Filter(..., POINT, NONE, POINT)`.

Выбрано явное описание ресурса в shader scripts. Централизованный `r_Sampler` с проверкой имени и alias был бы менее надёжен: Lua `_sampler()` сначала вызывает `r_Sampler(name, nullptr)`, а `_texture()` назначает alias позже. Строгая проверка alias пропустила бы эти вызовы; разрешение любого `nullptr` расширяло бы неявное правило, которое поздние Lua-модификаторы могли бы перезаписать.

Скрипты GL общие для desktop и GLES, но ресурс 1×1, поэтому ожидаемое значение сохраняется и на desktop. Это исходниковое исправление CS/CoP; **runtime этих двух игр не проверялся**.

Хеши одинаковы у всех трёх игровых копий:

| Файл | SHA256 |
| --- | --- |
| `clouds.s` | `e88efc776f6e087a36232015d333142eb05a4f311e3509d4febd4a5e22f3d6ba` |
| `combine_volumetric.s` | `7d964d48191f679d773c960143e62c5884427f635f329238c7b70afb74e58bb5` |
| `portal.s` | `1ab793c0d2befc4f9aff1579a9c5ba611989e7016bef39181cba308384e011c8` |

`android/app/build.gradle` упаковывает `res` в assets. `OpenXRayActivity.createGameDirectories()` извлекает каталог выбранной игры в её `gamedata`. Все девять APK assets проверены по хешам; три извлечённых SoC-файла на планшете также совпали. Дополнительная Java-логика или изменение пользовательских конфигов для sampler fix не потребовались.

### Другие R32F-ресурсы: границы аудита

- `rt_smap_surf` с R32F создаётся только вне `USE_OGL`, поэтому к данному GLES-пути не относится.
- `rt_half_depth` выбирает R32F только для NVIDIA vendor ID; найденные `s_half_depth` используют point sampler. Для остальных vendor выбирается R16F.
- Необязательный `rt_smap_depth_minmax` — другой R32F с линейным comparison sampler. Если этот путь включать, его совместимость требует отдельной проверки. Не заменять автоматически фильтры shadowmap по аналогии с 1×1 экспозицией: размеры и семантика другие. По этому checkpoint отдельной правки minmax нет.

## Сохранённая диагностика, выключенная в Normal

Диагностический **`-gles_angle_audit`** остаётся в исходниках для воспроизводимой дальнейшей отладки, но в финальных Normal-тестах отсутствовал. `-gles_highp_position` тоже отсутствовал; native log подтвердил `G-buffer position sampler: default precision`. Для работы исправления оба флага не нужны.

`src/Layers/xrRenderGL/glHW.cpp`:

- `AngleAuditEnabled()` распознаёт отдельный токен с границами по пробелу/tab/CR/LF, не подстроку или `=1`.
- `OnAngleAuditDebugCallback()` не вызывает GL; ограничивает payload 1024 байтами и использует независимые atomic квоты: 64 critical (error/undefined/high), 16 medium warnings, по одному suppression notice на категорию.
- Под явным флагом `CHW::CreateDevice()` устанавливает synchronous callback при доступном debug API и пишет нужные float/precision capabilities. Обычный путь не включает эти дополнительные diagnostic GL calls.

`src/Layers/xrRender_R2/r2_rendertarget_phase_luminance.cpp`, `phase_luminance::angle_audit::{Enabled,NextSample,CheckErrors,Probe}`:

- Не более восьми выбранных кадров на процесс, чтение до/после финальной редукции.
- Проверка собственного complete 1×1 R32F target, attachment identity, уровня 0, float component и отсутствия MSAA.
- Используется только implementation-reported float read pair (RED/FLOAT или RGBA/FLOAT).
- Сохраняются/восстанавливаются READ_FBO, его собственный READ_BUFFER, target READ_BUFFER, pixel-pack buffer и pack parameters. Draw FBO/attachments и RCache не перенастраиваются.
- Incoming GL errors маркируются отдельно; ошибки чтения/восстановления не превращаются в фиктивные значения. Probe синхронный и потребляет GL error state, поэтому это инструмент диагностики, **не режим замера производительности**.

В реальном audit было четыре сообщения **performance / HIGH `GPU stall due to ReadPixels`**. Их вызвало намеренное синхронное диагностическое чтение. Уровень HIGH не превращает performance notification в GL API error. API error в этом диагностическом захвате не наблюдался, target/readback/restore проверки прошли. При этом отсутствие ошибки в обычном Release-логе само по себе не доказывает корректность всех draw calls игры.

## Проверки исходников и сборки

Исходники проверены независимо от полного Gradle build:

- `W/work/angle-exposure-tests/run.py`: фактические production `Compile()` до/после + настоящие `r_Sampler_rtf/clf`, mock recorder. Шесть compile configurations (GL Android/GLES/оба/desktop, R2 desktop/Android), четыре элемента — **24 сценария**. Только ожидаемый sampler изменён. Реальный ARM64 Android translation unit проходит `-fsyntax-only` с release compile-database flags.
- `W/work/angle-exposure-tests/check_lua_contract.py`: точная проверка только девяти изменений байтов; настоящий Lua 5.1 minilua из существующего test environment выполнил девять новых и девять сохранённых исходных scripts. Pass/state и остальные samplers совпали; C++ setters и luabind registrations сверены с mock API.
- `W/work/angle-audit-tests/run.py`: 22 flag, 6 callback-filter, 7 message-length cases, независимые quota floods, восемь concurrent producers. Поток warnings не подавляет последующую настоящую ошибку; mixed flood даёт 64+16 деталей и два notices. Предыдущая ошибка shared quota сохранена как исторический repro.
- Luminance probe: 17 случаев, включая gate-off с нулём GL calls, разные READ/DRAW FBO, PBO/pack offset, RED/FLOAT, unsupported/errors и восстановление; фактический Android NDK syntax также проходит.

Host fixtures не заменяют драйвер и не доказывают качество остальных эффектов. Переносимые JSON summaries находятся в `R/evidence/angle-integration/`: `exposure-results.json`, `exposure-lua-contract-results.json`, `exposure-lua-source-hashes.json`, `angleaudit-callback-results.json`, `angleaudit-callback-android-syntax.json`, `angleaudit-luminance-results.json`.

Финальная сборка запущена из `W` командой `./work/build.ps1 -Variant angleexposure2`; `work/build-angleexposure2.log`: **BUILD SUCCESSFUL in 19s, 47 tasks, 20 executed**. Использовалась уже построенная закреплённая пара ANGLE; Linux toolchain/ANGLE заново не собирались. Пины остаются:

- ANGLE `e5640f4ec9ce5fec357186fd7fa40ad3030f2698`;
- depot_tools `a03dfbf7f2d1cefa2795d81d59ca4f16c8616a8f`;
- принятый raw export `/home/user/oxr-angle-linux-e5640f4ec9ce/export-arm64-final`;
- portable recipe `R/scripts/build_angle_pinned_linux.sh`, более полные указания — предыдущий ANGLE checkpoint.

`angleexposure2-inputs.json` — отдельный неизменяемый input freeze, включая C++/Java/CMake, диагностические исходники, девять скриптов, две JNI `.so`, wrapper/recipe и protected preexisting hashes. `angleexposure2-apk-report.json` подтверждает подпись v2, прежний сертификат, 29 ARM64 ELF с 16KB-compatible load segments, отсутствие direct graphics imports в engine modules, отсутствие system GLES/EGL dependency у renderer, неизменный engine `libc++_shared.so`, пустой GitHub token и совпадение frozen inputs. SDL сохраняет собственные internal 2D GL imports; ANGLE-only splash skip предотвращает смешанный SDL 2D splash путь.

Проверки APK были выполнены до установки, поэтому их исходные `installed=false` / `runtime_verified=false` сохранены. Они не противоречат отдельным последующим installation/runtime reports. Не переписывай принятые отчёты и APK; для будущих изменений создавай новый variant/freeze и согласованные пути verifier/installer. Повторная установка уже проверенного APK без причины не нужна.

Установка `install -r` сохранила UID10361, `user.ltx`, четыре savedgame files и два optional external configs; фактический установленный APK совпал по SHA256. Private launcher/touch preferences сохранены обновлением, но отдельного private backup/root-доступа не заявлялось. Сертификат SHA256: `606162651241a780913dcff90a4bdd997899cb5aad606c44273d2d0ad3d4e244`.

## Финальные проверки на устройстве: точный объём

Все три захвата относятся к **`u0_a361 - quicksave`, `l01_escape`**, а не главному меню. Logs содержат живой backend/provider, введённую `load_last_save` и завершение синхронизации. Root вручную просмотрел изображения сцены.

| Проверка | Приватный каталог в `W/work/angle-runtime/` | PID | Результат |
| --- | --- | --- | --- |
| Native GLES Normal | `angleexposure2-native-normal-20261020-155505` | 30256 | Сцена/освещение видны; `/system/lib64/libGLESv2.so`, Adreno 740 ES3.2 |
| ANGLE/Vulkan Normal | `angleexposure2-vulkan-normal-20261020-160227` | 31615 | Сцена/освещение видны; оба provider из APK `libGLESv2_angle.so`, live context `angle_vulkan` |
| ANGLE screen sleep/wake | `angleexposure2-vulkan-resume-20261020-160620` | **31615** | Картинка вернулась, `onPause`/`onResume`, тот же процесс |

Vulkan identity: ANGLE 2.1.1 git `e5640f4ec9ce`, OpenGL ES3.2, Vulkan1.3.128, Qualcomm Adreno740. Native и Vulkan `user.ltx` побайтно совпали: `a2e3c74b7615072e67592beee4f9121c51d9d2d9fba6a17d19af69973e2e8c96`. Additional args пусты, диагностические флаги выключены, position sampler default. Normal profile сохраняет ранее принятые оптимизации/фиксы; «пустые дополнительные аргументы» не означает отключение нормальных профильных flags.

Crash buffer в этих финальных захватах пуст. Это короткие stationary smoke tests, а не длительное прохождение, тест каждого вида света/тени/частиц или одинаковые performance runs. Время CPU scene из старых audit logs не равно GPU time и не переводится в FPS. **FPS, выигрыш FPS, GPU/CPU bottleneck и сравнительная энергоэффективность не измерены.**

Успешный sleep/wake при сохранённом процессе не доказывает восстановление после настоящего уничтожения GL/Vulkan context, Activity/process kill, смены поверхности во всех условиях или нехватки памяти. Полное context recreation остаётся нерешённым/непроверенным архитектурным ограничением.

Итоговая очистка подтверждена [angleexposure2-test-cleanup.json](../../evidence/angle-integration/angleexposure2-test-cleanup.json): штатный выход, `onDestroy`, четыре неизменных save files, Normal без дополнительных args; выбран backend `angle_vulkan`, экран спит. Не запускай тесты из факта наличия старого capture; сначала проверяй текущее подключение, состояние приложения и разрешённую пользователем задачу.

## Инструменты удалённого теста и извлечённые уроки

`W/work/test_angle_saved_scene.py` — вспомогательный smoke-test script для конкретного планшета/пути игры, не production-код движка. Он не устанавливает APK и не сохраняет игру; отказывается останавливать активную `OpenXRayActivity`. Force-stop используется только для неактивного launcher перед сменой GL providers. После capture оставляет игру работающей для просмотра, поэтому завершение проверяется отдельно.

Подготовительные неудачи автоматизации были исправлены в helper, а не в рендере:

- После открытия консоли активация SDL text input/Android IME асинхронна. Без короткого ожидания команда могла не вводиться. Сейчас есть двухсекундная пауза перед `input text` и проверка реального `ExecuteCommand ... (record_cmd=1)`.
- Нельзя отправлять команду при одном только появлении окна. Helper ждёт правильный effective backend, default position sampler и **первый реально отрисованный кадр main menu**; после load — `End of synchronization`.
- UiAutomator может возвращать hint пустого `EditText` в поле text. `edit_text()` различает hint и фактическое значение, поэтому placeholder не принимается за добавленный пользователем аргумент.
- Штатный quit/уничтожение Activity также асинхронны. Для смены backend и cleanup нужно дождаться фактического завершения, а не немедленно считать наличие Activity записью «игра ещё работает» или сразу force-stop её. Финальный cleanup подтвердил `onDestroy` и целостность сохранений.

Эти случаи не являются доказательством игрового краша, поломки консоли, зависшего рендера или потери сейва. Скрипт захардкожен под текущее устройство; другой агент не должен запускать его на другом телефоне без адаптации и проверки.

## Что дальше и что не закрыто

1. Когда пользователь вернётся, дать ему проверить **уже установленный** angleexposure2: движение, деревья/листва, фонарик и NPC/предметные тени, костёр, торговлю/квесты, взаимодействие с физическими объектами, более длинный игровой сеанс и обычное сворачивание. Собирать fresh evidence по конкретным дефектам, не включать diagnostic flags постоянно.
2. После визуальной приёмки провести контролируемое Native GLES ↔ ANGLE/Vulkan сравнение в одной сцене/камере, с одинаковыми resolution/render scale/effects/AA/flags/FPS limit, прогревом caches и сопоставимым thermal state. Нужны отдельные CPU/GPU timings, waits/allocations/compile hitches и frame-time distribution. Перезапуск процесса между backend обязателен. Сейчас данных для обещания прироста нет.
3. Сохранять Native GLES как рабочий recovery/control route. API29 ANGLE gate, процессный restart lock, Java preload transaction, shared GLAD engine/ImGui dispatch и Android splash skip остаются в силе. `angle_gles` как третий route в этом финальном тесте не проверялся.
4. Не считать закрытыми прежние отдельные темы: временная рябь alpha-листвы, качество/стоимость AA, просадки после физики/разрушения ящиков, прежние ragdoll anomalies, полное context loss/recreation, совместимость S26 Ultra и полная CS/CoP поддержка. Существующие mapfix/questlife/physics и графические исправления защищены freeze-хешами и не изменялись ради exposure fix.
5. В `bloom_luminance_3.ps` существующий вызов `clamp(rvalue,...)` отбрасывает возвращаемое значение. Это отдельная исходниковая проблема, **не исправлялась** и не выдаётся за доказанную причину blackout. Не объединять её исправление с уже подтверждённым sampler fix без отдельного требования/теста.
6. Нативный Vulkan backend не реализуется в рамках этой работы. Рабочий новый маршрут продолжает исполнять GLES API через ANGLE. Git commits/push, релизные архивы и публикация отложены пользователем; этот checkpoint не является разрешением их выполнить.

Сырые device logs, screenshots, configs и сохранения остаются в приватном `W/work/angle-runtime/`. В репозиторий помещены только summary JSON и hashes/ссылки. Не копируй приватные захваты или signing keys ради handoff.
