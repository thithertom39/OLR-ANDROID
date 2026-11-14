<!-- CORDON_GPU_20261022_FINAL -->
> Обновление 22.10.2026: последняя GPU-сессия завершена; сначала читать CORDON_GPU_PERFORMANCE_REPORT.md и NEXT_GPU_OPTIMIZATION_SESSION.md. Установлена cordonperf1, оба backend проверены, исходные настройки/сейвы восстановлены, планшет погашен. Контрольный Native FHD выигрыш +12.06%; P95/P99 почти без изменений. Git/push отложены.

## 2026-10-22 06:04 MSK — perf20261022 установлен, пользователь проверяет сам

По просьбе пользователя на подключённый Tab S9 установлен `org.openxray.dev 1.6.02-perf20261022` обновлением без удаления приложения. UID10361 и точный hash APK проверены; user.ltx, все4 save files и2 внешних конфига совпали. Backup прежней angleexposure2 и данных: `W/work/angle-runtime/install-perf20261022-20261022-060433`. Отчёт: `W/outputs/angle-work/perf20261022-installation-report.json`. Игра не запускалась, настройки/аргументы не менялись; runtime/FPS ещё не проверены. Пользователь просил закончить установку быстрее и будет тестировать всё самостоятельно. Следующий шаг — дождаться его наблюдений/сообщения о загруженном сейве; не запускать удалённые игровые тесты без необходимости. Нижний offline checkpoint является историей подготовки, его «не установлен» уже устарело.

---

## 2026-10-22 — OFFLINE_PERFORMANCE_CANDIDATE_READY_DEVICE_PENDING

**Актуальное состояние этой задачи; этот блок имеет приоритет над историческими указаниями ниже.** W = `C:/Users/User/Documents/Codex/2026-10-11/gjvyb`; R = `W/work/openxray-build`.

- Выполнена автономная offline оптимизация по пользовательскому вложению, без планшета/ADB/установки/запуска и без изменения разрешения/эффектов/AA/управления/AI/квестов/сейвов/глобальных настроек Windows. Native GLES + ANGLE Vulkan сохранены; нативного Vulkan нет.
- Готова отдельная APK `W/outputs/openxray-soc-perf20261022-arm64.apk`, `org.openxray.dev 1.6.02-perf20261022`, 33 696 780 bytes, SHA256 `7e12dac3fb52cc081ee3dc49e49641e58acc03725def2cbd2a01fc03e55fa6f3`. Build 2m42s PASS, lintVital/signature/29ARM64/16K/CRT/sharedGLAD/10543frozen source files PASS. 2197 game/render assets и ANGLE pair совпали с angleexposure2; отличается только AGP-generated ART `baseline.prof`, что отдельно проверено. **Candidate НЕ установлен, runtime/FPS/пиксели Adreno не проверены.** Последняя известная установка — angleexposure2, не считать её текущий статус повторно проверенным без подключения.
- Девять изменений в 13 production files: texture ClearStage known-clear proof; exact GLES drawBuffers cache с отдельной FBO invalidation; camera-only touch MOVE без лишнего overlay redraw; Java timestamp lock; SDL controller close; Android Release duplicate stderr opt-in `-stderr_log` с reinit reset; verbose UI callback traces opt-in `-ui_callback_trace`; reuse sound FS.exist; integer-bounded Vorbis read. Default graphics/launch settings не менялись, новые флаги не нужны для обычной игры.
- Проверки: actual-source differential fixtures, mutation tests, ASAN/UBSAN; texture2.38Mops/60profiles; draw450096ops eachGLES/Android/desktop и actual Mesa GLES20 pixel/MRT checkpoints+6errorcases; NDK full TU checks; Java actual-class traces1620ops/572visualchanges; concurrent formatter12000records; final active CPU suite PASS. Это локальные доказательства, не полноценный игровой test и не FPS benchmark.
- **Исключён** sound-cache publication patch: mockTSAN не покрывал plain refcounts реального `shared_str`. `src/xrSound/SoundRender_Core_SourceManager.cpp` exact baseline. Предложение сохранено как `W/work/perf-20261021/tests/cpu_resources/03-source-cache-owner.patch`, не возвращать без нового lifetime/race proof. Final active suite — `verify_active.py`; старый `verify.py` entry point включает отклонённый эксперимент и не годится как приёмка текущей production.
- Полный отчёт `R/ANDROID_PERFORMANCE_OPTIMIZATION_REPORT.md`; переносимые evidence `R/evidence/performance-20261022/`; первичные fixtures и логи `W/work/perf-20261021/tests/`. `W/outputs/angle-work/perf20261022-{inputs,apk-report,regression-report}.json` — offline proof, не install report.
- Откат pre-task пользовательского дерева (включая прежние незакоммиченные изменения): `W/work/perf-20261021/source-baseline.zip`, 11461files, SHA256 `39831b2f8b24b0c785be452fa33769d48cfd64da9116d0df70c51342e40c65c4`; `baseline-manifest.json`, `optimization-changes.json`, `optimization-vs-baseline.patch`. `restore_optimization.py` по умолчанию dry-run, `--apply` восстанавливает только13файлов после hashes/path/late-edits checks и backup. Испытан изолированными fixtures; реального отката не делали. Baseline APK angleexposure2 не изменена. Коммиты/push по-прежнему отложены.
- **Продолжать с device-проверки кандидата при возвращении пользователя:** backupAPK/config/saves/signature → updateinstall без uninstall → Native тот же save/настройки/камера, свет/тени/AA/UI/input/sound/trade/PDA/save/resume → force-stop/reopen → actualANGLE Vulkan и та же проверка. Затем сравнить baseline↔candidate отдельно на каждом API при одинаковых actual resolution/thermal/cache/refresh; отдельно сцена ящиков. 1080p preset Tab S9 раньше был1728×1080, не выдавать за1920×1080. Main menu не является игровымFPS тестом. Новых измерений пока нет, прирост не гарантирован.
- Не включать старые stream_orphan/stream_sync/state_rebind experiments для ускорения; angle_audit/readbacks только диагностика. Защищены preexisting FS/SkeletonRigid/GameTask и POINT R32F exposure/9Lua contracts.

---

## 2026-10-20 16:15 MSK — ANGLE exposure исправлена, финальные smoke tests пройдены

**Актуальный checkpoint: `ANGLE_EXPOSURE2_NATIVE_VULKAN_SMOKE_PASSED`. Этот блок имеет приоритет над всеми историческими «ждём angleaudit», «black world не исправлен», «APK pending» и указаниями о вчерашней паузе ниже.** W = `C:/Users/User/Documents/Codex/2026-10-11/gjvyb`; R = `W/work/openxray-build`.

- Уже собран и установлен `org.openxray.dev 1.6.02-angleexposure2`: `W/outputs/openxray-soc-angleexposure2-arm64.apk`, SHA256 `d31ef8e016b42f4d65efd81334f477fc6c098c018d210f9b3de58ae5b7826abe`. Финальный runtime summary: `W/outputs/angle-work/angleexposure2-runtime-report.json`, переносимая копия `R/evidence/angle-integration/angleexposure2-runtime-report.json`. Старые immutable build/install reports описывают свои этапы, а не текущую установку.
- Причина чёрного мира подтверждена: отсутствующее у ANGLE `OES_texture_float_linear` делало LINEAR чтение 1×1 R32F истории экспозиции неполным. Изолированный POINT fix в `blender_luminance.cpp`, R_GL case2, восстановил картинку; поздняя экспозиция ~1.514 вместо ~0.10 при прежних save/backend/config/flags. Финальная сборка дополнительно явно задаёт `clamp/f_none` в девяти Lua tonemap consumers (`clouds/combine_volumetric/portal`, soc/cs/cop). Тональная кривая/форматы/разрешение не менялись. Host/actual Android syntax, полный build, signature/29 ARM64 ELF/16K/CRT/shared GLAD/freeze и packaged+extracted SoC assets проверены.
- На Tab S9 SM-X710 загружен тот же `u0_a361 - quicksave`, `l01_escape`: Native GLES Normal (capture155505, PID30256) и **реальный ANGLE/Vulkan** Normal (capture160227, PID31615) показывают видимый освещённый мир; root просмотрел изображения. `user.ltx` одинаков, additional args пусты, position sampler default, `-gles_angle_audit` и `-gles_highp_position` выключены. ANGLE sleep/wake capture160620 сохранил PID31615 и видимую картинку. Это проверка одной сцены и одного возврата, не полного context recreation.
- **FPS/прирост/CPU–GPU bottleneck не измерены**, полный gameplay/effects/длительный сеанс, S26 Ultra и runtime CS/CoP не проверены. Native GLES сохранён. Нативный Vulkan renderer остаётся вне scope. Диагностический флаг retained, но off; HIGH performance `ReadPixels` notices в audit не были GL API errors, audit непригоден для FPS сравнения.
- Пользователь разрешил root удалённо управлять планшетом для этих тестов. Финальный cleanup: штатный выход, `onDestroy`, четыре сохранения неизменны, Normal без extra args, выбран `angle_vulkan`, экран спит; полного выключения нет. Пользователь отсутствует, активного игрового теста нет.
- **Продолжать с пользовательской проверки уже установленной сборки**: движение/фонарик/тени/костёр/взаимодействия/длинный сеанс, затем контролируемые Native↔ANGLE timings при одинаковых условиях и thermal state. Не переустанавливать без причины, не повторять старую диагностику/билд по устаревшим заголовкам. Полный checkpoint: `R/docs/development-checkpoint/ANGLE_EXPOSURE_FIX_2026-10-20.md`; там причины, точные файлы, доказательства, ограничения и уроки `test_angle_saved_scene.py` (IME/menu readiness/hint/cleanup wait). Git/push/релизное оформление по-прежнему отложены пользователем.

---

## 2026-10-20: angleaudit installed; waiting for diagnostic save load

- Valid test `-gles_highp_position` (capture `work/angle-runtime/angle-highp-position-corrected-20261020-135814`, PID19762) verified `highp (diagnostic)`, actual ANGLE Vulkan, loaded quicksave; scene still near-black. Precision change ALONE did not solve blackout. Earlier typo attempt remains invalid. Summary `outputs/angle-work/angle-highp-position-result.json`.
- New diagnostic-only APK `outputs/openxray-soc-angleaudit-arm64.apk`, 1.6.02-angleaudit, SHA256 fe83fc6aea044d2e645e97784e45dd39f4c5005e0ed4f0f32e79f000ddbc0dd5, 33694876 bytes. Gradle assembleRelease passed in56s; signature/29ARM64ELF/16K/sharedCRT/dispatch/frozeninputs/protectedmapfix files passed. Accepted original angle APK remains unchanged.
- User exited game normally; install-r completed 2026-10-20 14:27MSK, UID10361 retained, installed APK exacthash verified, user.ltx+4savedgame files+2externalconfigs unchanged. Backup `work/angle-runtime/install-angleaudit-20261020-142658`. Private launcher/touch prefs preserved by update, no root/private backup claimed. Separate report `outputs/angle-work/angleaudit-installation-report.json`; original angle install/build reports are immutable historical evidence.
- Code changes ONLY opt-in `-gles_angle_audit`: glHW.cpp exact-token flag, synchronous debug callback with independent64critical/16warning caps (warning flood cannot hide real errors), float-linear/color-buffer extensions and fragmentprecision logs; r2_rendertarget_phase_luminance.cpp reads owned complete1x1R32F target before/after final draw on max8frames, using actual implementation-reported RED/FLOAT orRGBA/FLOAT, restoring per-FBOREAD_BUFFER/PBO/pack state and original READ_FBO. No draw-FBO/attachment/cache mutations, no rendering-algorithm/shader changes. Readback consumesGLerrors and canstall: DIAGNOSTIC, never use for FPS comparison. Default normal path has no added diagnostic GLcalls.
- Focused actual-helper tests:22flag/6filter/7length cases, independentquota+concurrency, realAndroidNDKsyntax; luminance17cases include differentREAD/DRAW, boundPBO/packoffsets, REDread, error/unsupported paths, defaultoff0GLcalls, plusreal full.cppNDKsyntax. Reports in `work/angle-audit-tests`, portable JSON summaries in `evidence/angle-integration`; hostfixtures do not establish actualdriver correctness.
- Current task: user opens launcher, retains ANGLE Vulkan + `-gles_highp_position`, adds ONLY `-gles_angle_audit`, MAIN MENU then same quicksave, renders unpaused10-15s, root captures via `work/capture_angle_runtime.py`. Verify actualdiagnosticstartup+gate+restored=1 before interpretingvalues. No diagnostic game run yet; blackworld rootcause remains UNKNOWN.
- Hypothesis to assess from actualcaps/readback: R32F exposure feedback uses LINEAR and mayrequire unavailableOES_texture_float_linear. Not proven. Pinned ANGLE returns GL_RED/FLOAT forR32Factualformat (unlessfallbackformatdiffers), so probe supportsRED. Iffloatlinearabsent andexposurebad, consider nearestfilter on1x1feedback as minimalfix AFTER evidence; do notblindlyaltertonecurve/Gbuffers. Other candidates: sampledtexturecompleteness, FBOerrors, Gbuffer inputs. Existing negative tonemapoff test stilldoesnotexclude feedbackfailure.
- Build: `./work/build.ps1 -Variant angleaudit`; inputfreeze outputs/angle-work/angleaudit-inputs.json. Verifier `work/verify_angleaudit_apk.py --variant angleaudit --expect-angle present` (copyoforiginalwithdiagnosticpayloadchecks/currentdeviceauthorization); installer `work/install_angleaudit.py` guardsactivegame, backups, signature/hash/data checks. First preflight attempt used oldreport path and correctly refused reinstall before ANYdevice mutation; helperpath corrected, later install verified. Do not rerun installer when sameAPKalreadyinstalled.
- No Git commit/push/releasepackaging, no Windows globalsettingchanges, no WSL/ANGLErebuild; existingNativeGLES route preserved. NativeVulkan remainsoutofscope.

## Resumed 2026-10-20: highp-position attempt NOT active (argument typo)

User returned with save loaded; pause below is superseded. Fresh read-only capture `work/angle-runtime/angle-highp-position-20261020-135316` verifies ANGLE Vulkan, but Java launch arguments contain `-gles_highhp_position` (extra h) and native log still says `G-buffer position sampler: default precision`. World remains near-black. This is NOT a valid highp-position test and does not reject that hypothesis. Next: replace typo with exactly `-gles_highp_position`, restart, same ANGLE/save/settings; confirm native `highp (diagnostic)` before drawing conclusions. Report: `outputs/angle-work/angle-highp-position-attempt-report.json`. No renderer code, APK, device settings or configs changed in this read-only inspection.

## Paused by user after first ANGLE black-world test — 2026-10-19

- User explicitly paused all development until tomorrow. Do not resume autonomously; wait for the user.
- Installed APK remains `outputs/openxray-soc-angle-arm64.apk` (1.6.02-angle); no new renderer-code edits or rebuild were made during this diagnosis.
- Actual ANGLE -> Vulkan context and save load are verified, but scene visual acceptance FAILED (near-black world and sky, HUD visible). No FPS conclusion.
- Root performed a bounded runtime console test: `r2_tonemap off` was accepted in engine.log, console/IME were closed and the unpaused scene captured in `work/angle-runtime/angle-tonemap-off-live-20261019-222720`. Normal lighting did not return (dark geometry, grey veil/rain). This does NOT exclude exposure RT/sampler failure: the command still uses luminance feedback and previous R32F exposure.
- ORIGINAL `r2_tonemap on` was RESTORED, confirmed by ExecuteCommand + `@ r2_tonemap on` in the live engine log. No other graphics option was changed by root. The read-only capture helper's options_changed=false describes capture itself, not the preceding console experiment.
- Pending async request to test `-gles_highp_position` is suspended/cancelled by user pause. No such restart/test has yet been verified. Tomorrow, first check live args before assuming whether user later added it.
- Next controlled test after user resumes: append ONLY existing `-gles_highp_position` to launcher additional args, restart normally, keep ANGLE Vulkan + same save/config, verify `G-buffer position sampler: highp (diagnostic)` in fresh log and capture scene. Cache key includes the option, no shader-cache deletion needed.
- If that fails, add bounded opt-in diagnostics only after review: actual GL_OES_texture_float_linear support, framebuffer completeness/GL errors, exposure texture values and G-buffer samples. R32F exposure feedback currently uses LINEAR sampling (blender_luminance.cpp R_GL case2); pinned ANGLE requires OES_texture_float_linear for R32F linear completeness. Actual device extension support is UNKNOWN; do not assert this is the cause.
- Other source finding: bloom_luminance_3.ps ignores clamp() return. Confirmed code issue, NOT a proven cause of current blackout. G-buffer s_position defaults to lowp sampler while packed hemi/material values have abs32..8188; existing highp flag is only a hypothesis test. Matrix-transpose theory was checked against pinned ANGLE and rejected for ES3.2. Do not change transpose blindly. -novtf is not a clean exposure test because existing environment code binds sky textures only with VTF enabled.
- Reports: outputs/angle-work/angle-first-runtime-report.json and repository evidence/angle-integration summaries; private raw captures stay in work/angle-runtime. No game/device/Windows changes should continue during pause.

# PROJECT_STATUS — OpenXRay Android / S.T.A.L.K.E.R. Тень Чернобыля

## Подтверждён первый ANGLE/Vulkan runtime: контекст работает, картинка НЕ прошла проверку

19 октября 2026, захват22:11:30MSK (`ANGLE_FIRST_RUNTIME_2026_10_19`). На установленной `1.6.02-angle` (SHA256 `7557b74aaeeab740201b0cebce2500c3a89a45be11d28676aa084564bc7fb184`) пользователь загрузил тот же quicksave/l01_escape. **Реальный ANGLE → Vulkan1.3.128/Adreno740 подтверждён**: requested/effective `angle_vulkan`, оба GLAD provider из APK `libGLESv2_angle.so`, native log `verified live context: angle_vulkan`, загрузка сейва и конец синхронизации. Но мир чёрный, видны контуры: **lighting/gameplay visual validation FAILED; общий runtime_verified=false**. Успешная сборка/контекст не означают готовый рендер.

Отчёт `W/outputs/angle-work/angle-first-runtime-report.json`; переносимые summary+hashes в `R/evidence/angle-integration/`. Приватные raw logs/config/screenshot остаются только в `W/work/angle-runtime/angle-black-lighting-20261019-221130`; они не копируются в репозиторий. Native baseline находится в `W/outputs/angle-work/native-baseline-report.json`. Хеш user.ltx совпадает; 1728×1080/render_scale1,Extreme,FXAAFast/on,Normal,unlockoff. ANGLE значительно теплее Native-снимка; **FPS/GPUtime не измерялись**, выводов об ускорении нет.

Готовые ANGLE библиотеки уже собраны/упакованы/установлены; старые разделы ниже про «ANGLE не запускался», неподключённый планшет, отсутствие WSL или SOURCE_READY_APK_PENDING — история. В данном захвате нет fatal/GLSLcompile/FBO/OOM/context-loss сообщений, но Release непрерывно GL/FBO не проверяет. Наличие RGBA16F caps не подтверждает корректность R32F luminance/filtering. Exposure/tonemap feedback и `-gles_highp_position` — **только гипотезы/контрольные тесты**, не исправления. Root выполняет ограниченный tonemap-off → restore-on тест; его результат на момент этого checkpoint ещё не получен. Следующий шаг — диагностировать чёрное освещение, сохранять Native GLES и текущие шейдерные/теневые исправления; не переходить к обещаниям FPS или нативному Vulkan.

## Последний runtime: Native-сейв загружен, ANGLE ещё не запускался

19 октября около22:01MSK пользователь загрузил quicksave/l01_escape на новой angle APK. Контрольный Native GLES3.2/Adreno740 подтверждён requested/effective и обоими GLAD providers `/system/lib64/libGLESv2.so`; screenshot показывает игровой уровень, не меню. Свежих fatal/GL/FBO/OOM/context-loss ошибок в захвате нет. Но во время загрузки был watchdog-простой12–26s: sampled stack — fstatat64/CLocatorAPI::exist/CSoundCollection/CSoundPlayer/Lua add_sound/spawn; синхронизация завершилась22:00:06, дальнейшая активность продолжилась. Это не доказанный GL stall и не постоянное зависание. Есть179 повторов DetailPathManager и Blender metadata/Lua diagnostic warnings; причины AI/звуковых file probes исследовать отдельно позднее, не менять сейчас при переключении рендера. PSS~2GB — одиночный снимок, не доказательство отсутствия утечки. FPS не измерялся.

Отчёт `W/outputs/angle-work/native-baseline-report.json` и переносимая копия `evidence/angle-integration/`; raw logs/config/screenshot — `W/work/angle-runtime/native-baseline-20261019-220113`. Настройки сохранены:1728×1080/render_scale1,Extreme,FXAAFast/on,Normal,unlockoff. Следующее действие: пользователь штатно выходит из игры, затем restart процесса и выбор ANGLE Vulkan, тот же quicksave. **ANGLE/Vulkan runtime пока false.**

## Последнее обновление: angle установлена на Tab S9, 19 октября 21:56 MSK

Пользователь вернулся и подключил планшет, отменив прежнее ожидание вечерних тестов. На SM-X710/R52X40ES4LE установлена `org.openxray.dev 1.6.02-angle`, SHA256 `7557b74aaeeab740201b0cebce2500c3a89a45be11d28676aa084564bc7fb184`. До `install -r` сохранены реальная mapfix APK, `user.ltx`, все savedgames и два дополнительных внешних config-файла; подпись совпала. После обновления UID 10361, хеш скачанной установленной APK, user.ltx/4 файла сохранений/2 дополнительных config-файла проверены. Backup `W/work/angle-runtime/install-20261019-215625`, отчёт `W/outputs/angle-work/angle-installation-report.json`. Private launcher/touch XML отдельно не копировались (release без root/run-as); данные сохраняются механизмом обновления без uninstall/clear.

Лаунчер открыт и прочитан через UI XML; выбран **Native OpenGL ES**, 1728×1080, Extreme, FXAA on/Fast, Normal, unlock FPS off. Старое сообщение о вылете 15 октября пропущено, это не новый вылет angle. Игра ещё не запущена: дальше пользователь нажимает MAIN MENU и загружает тот же сейв для контрольного Native-запуска, затем restart процесса и ANGLE/Vulkan. Реальный Vulkan, качество и FPS ещё не проверены. Исторические `installed=false` в offline build/evidence относятся к моменту сборки; текущую установку описывает отдельный installation report. Пункты ниже о необходимости дождаться подключения устарели.

## Текущая работа 19 октября 2026: ANGLE → Vulkan

Пользователь разрешил реализацию **существующего GLES-рендера через ANGLE → Vulkan**; нативный Vulkan-бэкенд сейчас не создаётся. Native OpenGL ES необходимо сохранить. Установка и игровые тесты отложены пользователем до возвращения с работы: ночью планшет/ADB не трогать. Коммиты, публикация и упаковка релиза также не разрешены в текущем этапе.

**APK_READY_DEVICE_PENDING.** WSL2/Ubuntu-24.04 работают; Linux workspace `/home/user/oxr-angle-linux-e5640f4ec9ce`, VHDX на D:. Закреплённый ANGLE успешно скомпилирован (1674 операции), экспортирован и проверен: ARM64, API29 public symbols, 16KB, SONAME, runtime dependencies, статический Chromium libc++. Прежний C++ runtime движка не заменён. Финальная APK **`W/outputs/openxray-soc-angle-arm64.apk`**, 33692484 байта, SHA256 **`7557b74aaeeab740201b0cebce2500c3a89a45be11d28676aa084564bc7fb184`**, `org.openxray.dev 1.6.02-angle`, полностью собрана за 2m19s и проверена. 29 ELF/16KB, signature v2 и сертификат mapfix, ZIP CRC, dependency closure, отсутствие direct GL imports в движке, splash guard, пустой GitHub token и сохранность исходников/пяти прежних защищённых файлов — passed. Отчёты `W/outputs/angle-work/angle-{export,apk}-report.json`, freeze `angle-inputs.json`. В APK входят и последние fail-fast/splash правки. Историческая angleprep не содержит ANGLE и больше не является кандидатом для Vulkan-теста.

**Финальная angle APK не установлена; реального Vulkan-контекста, изображения, FPS и lifecycle ANGLE пока никто не проверял.** Следующий шаг после возвращения пользователя — backup/mapfix → установка angle без удаления данных → Native GLES smoke test → force-stop/reopen → выбор ANGLE Vulkan и тот же загруженный сейв, затем correctness/lifecycle и только потом сравнение производительности. Выбор через лаунчер; дополнительных ANGLE-флагов не нужно. В одном процессе менять backend нельзя. Последняя подтверждённая установленная сборка остаётся mapfix, `W/outputs/openxray-soc-mapfix-arm64.apk`, SHA256 `92850611d279bca7a9a5d5325ec22f579b482ceb496a2015636f300bb369f9a9`. Причина растянутой модели плоти по-прежнему не установлена; ANGLE не объявлен исправлением этого бага.

Подробное актуальное состояние, причины неудачных промежуточных шагов, точные команды и границы доказательств: [ANGLE_INTEGRATION_2026-10-19.md](docs/development-checkpoint/ANGLE_INTEGRATION_2026-10-19.md), особенно финальное приложение. Переносимый проверенный рецепт — `scripts/build_angle_pinned_linux.sh`; завершённый Linux export — `<workspace>/export-arm64-final`. Локальные журналы `W/outputs/angle-work/`; активных сборок после завершения этого этапа нет. Новые JNI `.so` находятся в `android/app/src/main/jniLibs/arm64-v8a/`, но папка игнорируется Git: обычный clone их не переносит. Ни коммитов, ни публикации не было. Глобальные настройки Windows не менялись. Разделы ниже — история; верхний блок и последний проверенный отчёт имеют приоритет.

## Актуальное состояние: questlife, исправление жизненного цикла заданий

SOURCE_READY_APK_PENDING. После успешной сдачи предметов пользователь обнаружил, что Медуза Сидоровича и куртка Шустрого остаются активными в ПДА после награды; у Волка не засчитываются кабаны через дорогу. В GameTask.cpp исправлены две воспроизведённые ошибки: пустой helper больше не стирает XML-функции SoC; при загрузке восстанавливаются функции Lua-подзадач. Формат сейва, Lua-скрипты, награды и условия целевых групп не изменены. 15/15 lifecycle-тестов passed; старый код воспроизводит 13 дефектных случаев. Прежние dispatch/item проверки и обе ARM64 syntax TU passed.

Уже оплаченные Медуза/куртка в offline save/load завершаются без нового предмета и второй награды. Реальные ПДА/сейвы пользователя пока не проверены. Для кабанов Волка исправлена общая потеря функций после reload; фактическая целевая группа и её population пока неизвестны. Не объявлять проблему кабанов полностью закрытой до теста.

Подробности и границы доказательств: docs/development-checkpoint/QUEST_LIFECYCLE_2026-10-15.md, evidence/questlife-tests. Текущая APK на планшете всё ещё nightpolish. Daypolish уже собрана offline; её тёмные панели, Fine edges, physics/cache изменения входят и в новый questlife candidate. Следующий шаг — полная отдельная offline сборка questlife, затем вечером backup/install и проверка существующих заданий. Планшет/ADB до возвращения пользователя не трогать. Разделы ниже сохранены как история; это верхнее обновление и последний build manifest имеют приоритет.


## Результат полной сборки daypolish

Полный Release APK готов: `outputs/openxray-soc-daypolish-arm64.apk`, 30953911 байт, SHA256 `a6a6bf1d36f87e76ed0bb61577f02f07b4a2da0adbe20819e04bf8fdd516016c`; package org.openxray.dev, version1.6.02-daypolish. Сборка/Java/native linking/lintVital завершились успешно за 2m 43s. Проверены ZIPCRC,27ARM64 ELF/16KB, signaturev2 и совпадение сертификата с установленной nightpolish. Все 11405 замороженных tracked inputs, dependency heads и build helpers совпали после сборки. Проверены новый resource/DEX selector, точный shader asset, native physics log payload и побайтно сохранённый wall внутри APK. BuildConfig GitHub token пуст. Новая APK НЕ установлена, device/runtime/FPS не проверены.

Дополнительный полный `lintRelease` НЕ прошёл:32 errors/149 warnings. Все32 ошибочных statements присутствовали в baseline f7929b7 (26MissingPermission,4NewApi,1receiverFlag,1RestrictedApi); новых errors в добавленных AA/physics/cache функциях не выявлено. Большинство относится к SDL Bluetooth/capture/vibration и API26 helper classes, отдельное прежнее замечание — AppCompatButton support background tint. Это не объявляется «чистым lint». Проверка release/lintVital при этом passed. Подробнее daypolish-lint-review.json, outputs/daypolish-lint-results.xml/.txt/.html и итог аудита SDL ниже/в evidence. Не подавлять lint целиком и не добавлять runtime permissions вслепую ради зелёного отчёта. Новая проверенная граница — compilation/artifact/source review, а не полнота runtime тестов.

Следующий шаг вечером: guarded backup/install готового daypolish; визуально панели, AA roundtrip/None/preset/resume, same-scene Fast/Fine A/B; Diagnostic + explicit -physics_audit в первую минуту симуляции около ящиков. Rollback nightpolish сохранён. Доказанного исправления конкретной просадки пользователя или прироста FPS пока нет. WSL отдельно ещё не установлен; инструкция `docs/development-checkpoint/WSL_SETUP.md` / `outputs/WSL_SETUP.md`.


## Текущая работа 15 октября: daypolish, исходники готовы к сборке

APK_BUILT_OFFLINE_DEVICE_PENDING. Пользователь поручил убрать разбитое стекло при сохранении рамок, доработать AA/оценить листву и проверить ошибки/производительность, особенно FPS после разбивания ящиков. Реализованы тёмная заливка панелей, явный Fast/Fine edges, исправление fallback lifetime обломков и согласованности/безопасности shader cache, optional -physics_audit. Полная новая APK собрана и проверена на ПК, но не установлена на планшет. Nightpolish остаётся установленной рабочей базой с положительным отзывом пользователя. Планшет до вечера не трогать.

Полный актуальный перечень решений, путей, доказательств, ограничений и вечернего сценария: `docs/development-checkpoint/DAYPOLISH_2026-10-15.md`. Он имеет приоритет над историческими формулировками «Fine edges не реализован» ниже. Источники/лёгкие отчёты в evidence/{daypolish-launcher-tests,dark-panel-tests,fineedges-tests,physics-stall-audit,shader-cache-audit}. Циклические квесты и ранее проверенные native fixes не переписывались. Следующий шаг — вечером установить готовую daypolish и проверить изображение/AA/физику; ADB/визуальный/FPS тест только при возвращении пользователя.


## Последний результат: положительный тест пользователя

15 октября 2026 пользователь сообщил: «затестил пока был на работе, все заработало, отлично». Это ответ после установки nightpolish и просьбы проверить сдачу Медузы Сидоровичу и куртки Шустрому. Первичная проверка исправления квестов считается успешной **по сообщению пользователя**; отдельные квестовые сценарии в ответе не перечислены. Новых ADB-команд, игровых действий или инструментальных замеров в этот момент агент не выполнял.

Nightpolish сохраняется как рабочая сборка, проверенная пользователем. Нельзя из этого общего ответа выводить независимую проверку награды строго один раз, циклического reset/save-load, всех режимов FXAA/профилей, торговли у остальных NPC, длительной стабильности либо прироста FPS. Предыдущие формулировки «ждём первичного теста» ниже относятся к состоянию перед этим сообщением. Следующий запланированный этап — отдельный опциональный Fine edges для листвы при сохранении Fast по умолчанию; режим пока не реализован. Отчёт: `outputs/nightpolish-user-test-report.json`, переносимая копия в `docs/development-checkpoint/`.


## Последнее обновление: установка nightpolish на Tab S9

15 октября 2026, 05:29 MSK: пользователь подключил планшет и разрешённый этап установки выполнен. На SM-X710/R52X40ES4LE установлена **org.openxray.dev 1.6.02-nightpolish**, UID 10361, SHA256 `3d499cbd74bc7f113132b15675db4e43c66c7e87f1977eea56f1cb3cb3c0f5d1`. Подписи старой и новой APK совпали; APK устройства повторно скачана и её SHA проверен. До установки сохранены предыдущая zoneskin3 APK, user.ltx и savedgames; после обновления user.ltx и все 4 файла сохранений совпали по SHA. Резервная копия `W/work/nightpolish-runtime/install-20261015-052900/` приватная и не включается в Git/экспорт.

В процессе оставался только неактивный LauncherActivity; игровой OpenXRayActivity в живом процессе не было. После backup/certificate checks закрыт лаунчер, выполнен install -r без удаления данных. Новый LauncherActivity открыт; реальный screenshot `W/work/nightpolish-runtime/launcher-first-open/launcher.png` просмотрен. Все три новых элемента профиля/FXAA видимы целиком в исходной компоновке планшета. Выбран Normal (play), FXAA off соответствует фактическому r2_aa off. Пользовательские игровые параметры не переключались, MAIN MENU/START NEW GAME не нажимались, сейв не загружался.

**Следующий шаг:** пользователь открывает MAIN MENU и загружает существующий сейв; проверяет сдачу Медузы Сидоровичу и куртки Шустрому. FXAA можно включить галочкой перед запуском; Normal сам добавляет рабочие флаги, чистить прежнюю строку Additional arguments не требуется. Проверка награды один раз, quest reset/save-load, FXAA/profile roundtrip и FPS ещё не выполнена. Пункты ниже с installed=false относятся к моменту первоначальной сборки, а не к текущей установке.

Отчёт `W/outputs/nightpolish-installation-report.json`; переносимая копия в `R/docs/development-checkpoint/`. Состояние installed=true, launcher_runtime_verified=true, runtime_verified/gameplay_verified пока false. Скрипт `W/work/install_nightpolish.py` и его копия в build-tools фиксируют процедуру; повторно запускать его на уже установленном nightpolish не нужно.

## 0. Назначение checkpoint и самая важная граница состояния

Документ создан 14 октября 2026 по просьбе пользователя для передачи разработки новому Codex/ИИ. Часовой пояс пользователя Europe/Moscow; точная машинная инвентаризация и Git-коммиты записаны в `outputs/handoff-checkpoint.json` рабочего каталога. Документ основан на чтении реальных исходников, Git status, сохранённых манифестов и результатов тестов. Автор не подключался к планшету во время последней ночной работы.

**На планшете теперь установлена 1.6.02-nightpolish.** Исправление циклических квестов, новые профили запуска, независимый переключатель FXAA и исправление записи настроек реализованы и проверены отдельными тестами на ПК, и теперь вошли в отдельную полную APK 1.6.02-nightpolish. Сборка и упаковка проверены на ПК; в игре новые изменения ещё НЕ проверены. Не выдавать старый zoneskin3 за сборку с этими исправлениями.

Новая доработка ряби листвы остановлена на исследовании: обоснован план отдельного опционального режима Fine edges, но ни новый console command, ни uniform, ни переключатель режима НЕ реализованы. Проверенный мобильный FXAA сохраняет 0.35/0.166/0.0625. Шейдеры во время последнего foliage-аудита не изменялись.

После сохранения checkpoint пользователь разрешил продолжить при оставшихся 5% лимитов. Выполнен ограниченный следующий этап: собрана и проверена отдельная nightpolish APK; новые изменения рендера не добавлялись. Текущая точка продолжения — проверка готовой сборки на планшете после возвращения пользователя.


### 0.1. Обновление после checkpoint: полная сборка nightpolish готова

Это обновление имеет приоритет над историческими формулировками «unbuilt/ещё не собрано» ниже. 15 октября 2026 выполнена сборка из commit `4dee003158c57f170c70bae2df9e7f5d6a850e5e`, GameSpy `ea81cef2ba519c593d4eccdd414da243ef17b061`:

- APK: `W/outputs/openxray-soc-nightpolish-arm64.apk`, 30 951 359 байт, SHA256 `3d499cbd74bc7f113132b15675db4e43c66c7e87f1977eea56f1cb3cb3c0f5d1`.
- Пакет `org.openxray.dev`, versionName `1.6.02-nightpolish`, versionCode 1; подписан существующим локальным тестовым ключом. Планшет не подключался: installed=false/runtime_verified=false. На устройстве по-прежнему zoneskin3.
- Native Release и Java compilation, Android resources, lintVital, dex и упаковка завершились успешно за 2m24s. 27 ARM64 ELF проверены на 16 KB PT_LOAD, ZIP CRC и APK signature v2 прошли. Проверены новые launcher resource IDs и маркеры профиля/флагов в DEX. Это не проверка качества картинки, FPS или выполнения задания на устройстве.
- До сборки заморожены хеши 11 365 tracked файлов с рекурсивными submodules, основной commit/dependency heads, build.ps1 и android/local.properties. После сборки совпали; все 97 ранее изменённых исходных файлов также совпали с checkpoint. Сборка не изменила код ради документации.
- Новый helper `W/work/nightpolish_pipeline.py prepare|verify` заменяет старые variant-limited упаковщики для этой сборки. При первом packaging исправлен путь к корню GameSpy (`Externals/GameSpy`, а не вложенный src/GameSpy) и сравнение Windows path ключа helper. Замороженный manifest не переписывался; manifest явно хранит старый/новый SHA helper. Ни исходники, ни команды сборки от этих исправлений не менялись. Повторная verify допускает только уже экспортированный APK с точно тем же SHA.
- Артефакты: `outputs/nightpolish-build-manifest.json`, `nightpolish-build-inputs.json`, `nightpolish-native-debug-symbols.zip`, `nightpolish-build.log`, `nightpolish-signature.txt`, `nightpolish-badging.txt`, `openxray-nightpolish.patch` и отдельный `openxray-nightpolish-gamespy.patch`. Оба cumulative patch прошли reverse apply --check на текущем checkout. Основная база 4223fa91..., GameSpy база 3e434803...; main patch не включает gitlink, поэтому отдельный GameSpy patch обязателен при применении к upstream dependencies.
- Новых renderer/shader/Fine edges функций в этой итерации нет. Исправление SoC callback и новые launcher функции уже скомпилированы в APK, но существующие квесты Medusa/куртка, сохранение FXAA, переходы профилей, реальная компоновка и Android AtomicFile требуют планшета.

При переносе helper-скриптов из `R/docs/development-checkpoint/build-tools/` скопировать их в `W/work/`: build.ps1 и Python helpers используют расположение относительно work, не являются автономными командами из docs. Исходный freeze относится к source commit сборки; последующий commit меняет только документацию/копии инструментов. Не запускать prepare повторно с перезаписью существующего freeze и не выдавать неподключённое устройство за проверенное.

## 1. Цель, ограничения и рабочая среда

Цель: нативный Android-порт OpenXRay для оригинальной Тени Чернобыля 1.0006 без эмуляции Windows. Основное устройство Samsung Galaxy Tab S9 SM-X710, позднее Samsung S26 Ultra. При различиях платформ приоритет планшету. Пользователь имеет файлы игры и сохранения. Файлы оригинальных game archives не входят в публикуемые исходники.

ПК пользователя Windows11, PowerShell. **Не менять Windows registry, драйверы, мышь, глобальные переменные/шрифты, автозапуск, таймер выключения и системные настройки.** Пользователь отдельно просил не портить Windows. Все инструменты и изменения окружения для сборки локальны процессу/папке work. Ошибка с чувствительностью мыши была исправлена пользователем самостоятельно; доказанного отношения к коду порта нет.

Пользователь разрешил принятие лицензии Android SDK, установку SDK/NDK/CMake/JDK, локальные сборки и согласованные ADB-обновления с сохранением данных. Не публиковать код, PR, APK или отчёты от его имени без соответствующего поручения. Форк пользователь создал сам; URL форка не сообщён. `origin` всё ещё указывает на исходный чужой репозиторий. Ничего туда не push.

Планшет на ночь отключён. **Не выполнять ADB, игровые замеры/запуски/установки до возвращения пользователя.** Пункт 2 плана (повторные проверки вылетов/подвисаний и замеры на устройстве) пользователь исключил из этой ночи. На ПК разрешены сборка, статические/host-тесты и исследование сохранённых материалов.

### Пути

Рабочий каталог чата `W = C:/Users/User/Documents/Codex/2026-10-11/gjvyb`.

Git-репозиторий `R = W/work/openxray-build`; именно здесь лежат основные PROJECT_STATUS.md и CONTINUE_PROMPT.md. Копии этих документов находятся в W для удобства нового диалога. База репозитория `4223fa91aaf443e6c71cba2edeb3ac84fb74177e`, upstream https://github.com/Eternal-Entropia/openxray-android. Локальный checkpoint сделан поверх этой базы; смотреть `git log -1`, `W/outputs/handoff-checkpoint.json`. До checkpoint накопленные изменения были unstaged/untracked, коммитов нашей работы не было.

Основные результаты `W/outputs/`. Runtime-доказательства и инструменты `W/work/`. Оригинальные APK пользователя: `C:/Users/User/Downloads/app-release.apk`, `app-release (1).apk` (вторая версия автора 1.6.07). Эти APK не являются нашими текущими исходниками.

## 2. Архитектура и технологии

Движок X-Ray/OpenXRay: C++17, CMake/Ninja, LuaJIT/Lua5.1 и luabind. Native-библиотеки собираются ARM64-v8a. Android лаунчер и Activity на Java/AppCompat, XML ресурсы, JNI и SDL2 Android integration. Есть SoC/CS/CoP режимы, но фактическая работа/проверки относятся прежде всего к SoC.

Рендер: существующий OpenGL backend, адаптированный под OpenGL ES3.x (`XR_PLATFORM_ANDROID`/`XRAY_USE_GLES`). Используется deferred pipeline с G-buffer, sun cascades и spotlight/flashlight shadow maps. GLSL-шейдеры и Lua overlays в `res/soc/gamedata/`, устанавливаемые поверх пользовательской игры. **ANGLE/Vulkan ещё не подключён к нашей APK.** Исходный проект содержит заготовки/скрипты ANGLE, но наличие scaffolding не равно собранному/проверенному Vulkan backend. GLES остаётся контрольным рендером.

APK `org.openxray.dev`, label OpenXRay Test; исходный `org.openxray` остаётся отдельным пакетом. Base version1.6.02 + test suffix. MinSDK24/targetSDK34/compileSDK34. Сборка содержит 27 ARM64 ELF `.so`. Для всех проверенных APK подтверждены PT_LOAD alignment>=16384, соответствие offset/vaddr и APK signaturev2. Планшет имеет PAGE_SIZE4096; фактический запуск на 16KB Android не проверен.

### Зависимости и структура

- `android/`: Gradle wrapper8.2, AGP8.2.2, app/build.gradle, Java/JNI/activity/resources.
- `src/xrEngine`: основной цикл, lifecycle, watchdog, console/frame profiler.
- `src/xrGame`: игровой код, AI, inventory/trade, quests, UIActorMenu, Lua exports.
- `src/Layers/xrRender`, `xrRenderGL`, `xrRenderPC_GL`, `xrRender_R2`: общий/GL рендер, buffers, state/samplers, lights/shadows/combine.
- `res/soc/gamedata`: SoC Lua/GLSL/UI compatibility overlays; `res/cop/`/`res/cs/` другие режимы.
- `cmake/`: конфигурации, Android dependency FetchContent и toolchain настройки.
- `Externals/`: GameSpy, LuaJIT, luabind, gli, imgui, sse2neon, xrLuaFix + встроенные OPCODE/ode/LuaJIT-proj.
- `oxr_deps/lzo-src`:457 отслеживаемых исходников, не удалять как build cache.
- `sdk`, `doc`, `misc`, `.github`: исходные SDK headers/tools/docs/CI.

Используемые семь gitlinks до checkpoint: GameSpy3e434803d95c206c065c6172536b45c357bf3e21; LuaJIT5a5cd82e435a7b08d61b4e0af887493d24d9eca1; gli3542f8830178061e0661f5df1e89d36ba7d7b0ab; imguifd13a1e8923a0a7077b404fc36fd063b25a0c0b5; luabind8da131b830f2615115c65438d48ac52cb9b7fa00; sse2neon13a42df35dc7fcc94f987568e7274a998bb6cc86; xrLuaFix0e89050f39fc1e6b21b856ed8b7a3d68ff486ccb. AGS_SDK/sse2rvv упомянуты в .gitmodules, но не являются используемыми initialized gitlinks этой Android-сборки.

GameSpy Android cleanup исправлен в `Externals/GameSpy/src/GameSpy/common/linux/gsThreadLinux.c` (19 добавленных строк): Android без pthread_cancel; завершение/ожидание joinable lifetime до освобождения handle. Ожидание getaddrinfo может быть долгим — это не исправление FPS. При checkpoint создан локальный submodule commit **ea81cef2ba519c593d4eccdd414da243ef17b061**. Он ещё НЕ доступен на upstream remote; учитывать при переносе/публикации gitlink.

FetchContent использует Ogg1.3.5, Vorbis1.3.7, LZO2.10, OpenAL Soft1.23.1 и SDL2.30.12. Депенденси исходники/кеши доступны на текущем ПК; новый ПК потребует интернет/SDK. Gradle wrapper JAR — обязательный отслеживаемый файл, не исключать вместе с бинарными build artifacts.

## 3. Что реализовано раньше и реально проверено

### 3.1. Android/Windows сборка

`CMakeLists.txt`, `cmake/XRay.Android.cmake`, `Externals/LuaJIT-proj/CMakeLists.txt`, `android/app/build.gradle`: локальный Android dependency layout, Windows object path limit240, LuaJIT получает текущий NDK sysroot. Host minilua/buildvm при отсутствии prebuilt `.exe` собираются MinGW/gcc; это host binaries, не NDK target executables. ARM64 release ThinLTO и max/common-page-size16384 задаются до создания FetchContent targets; ранее 6 внешних библиотек оставались 4KB.

У APK отключены вложенные реальные токены issue reporter: generated Release BuildConfig.GITHUB_TOKEN подтверждён пустым при source export. Не добавлять токены в код/коммит. Reporter UI может отправлять сообщения только по явному действию пользователя; агент его не нажимал.

### 3.2. GLES state/shadow correctness

Физическое GL state cache общее для backend instances, поскольку драйвер видит один context. Файлы: `glBackendStateCache.h`, `glTextureBindings.h`, `glState.cpp/.h`, `glR_Backend_Runtime.h`, `glSH_Texture.cpp`, связанные R_Backend files. Cache tracks actual programs/FBO attachments/VAO/buffers/texture units/samplers and avoids redundant driver calls; transitions/reset/old-tail cleanup сохраняются. Actual-header differential host tests покрыли 15k texture transitions.

`-gles_shadow_detach`: отключает оставшиеся color attachments в depth-only shadow pass. GPU-проба на TabS9 доказала: color attachment32x24 при depth64x64 ограничивал draws768 пикселями; без color attachment обновлялись 4096. Clear depth сам по себе не был доказательством правильной растеризации. Код `r3_rendertarget_phase_smap_D.cpp`, shadow pass helpers. Драйвер пропускает GL error для некоторых неверных state combinations — нужны фактические GPU query/probes.

Исправлен viewport/render scale crop: рендерится явный полный render-sized viewport; лаунчер выбирает абсолютное разрешение и записывает render_scale1. Старое render_scale0.45 давало 1152x720 при экране 2560x1600 и показывало только угол меню. Full frame проверен.

### 3.3. Динамические тени фонарика

Причина отсутствия opaque caster depth: пять GL shadow passes использовали null pixel shader/program0. Реальный Adreno probe: program0 оставлял clear depth без GL error, валидная dumb fragment program писала depth. Исправлены GL варианты в:

`blender_deffer_flat.cpp`, `blender_deffer_model.cpp`, `Blender_BmmD_deferred.cpp`, `Blender_Model_EbB_deferred.cpp`, `Blender_tree_deferred.cpp`.

Используется существующий dumb.ps; не удалять как якобы ненужную пустую fragment stage. Сборка spotfix проверена в исходном quicksave:63 writer draws с linked nonzero programs,49 actual GPU WVP matrices точно совпали CPU, две depth maps заполнены~95.6%. Пользователь подтвердил тени NPC/предметов от фонарика. Все материалы/источники/локации/устройства глобально не проверены.

Диагностические helpers `glSpotShadowDump.h` и `glSpotWriterAudit.h` используют bounded captures, проверяют selftest/physical state restoration, запрашивают actual GPU bindings/uniforms. Дамп float depth упаковывается GPU как exact binary32, не путать со скриншотом теней. Dumps2 карты; writer8passes. Эти flags не являются обязательными для исправления opaque casters.

### 3.4. Dynamic geometry / FPS

Стабильный opt-in `-gles_stream_upload` (`glStreamSync.h`, `glBufferUtils.cpp`, `R_DStreams.cpp`, `r__sync_point.cpp`) использует CPU upload динамических buffers и обходит конфликтующий fence path. Сохранять buffer identity referenced VAO, корректные offsets/storage sizes. Численные FPS сравнения без одинакового ракурса/фонарика/NPC/нагрева не причинное доказательство.

`-gles_uniform_batch`: передаёт точно прежние vec4 строки костей массивом вместо отдельных calls. `glUniformBatch.h`, `glr_constants_cache.h`, `SkeletonX.cpp`. Guards:1..78bones,3rows/bone, правильный float/RC_dest_all/shape/program/location; fallback старый. Host2370 bit-exact cases. На устройстве подтверждены 45bones/135rows и 18bones/54rows, пользователь не увидел деформаций/скачков/NPC/оружия/теней и независимо сообщил небольшое улучшение стабильности. **Пользователь просит оставить flag.** Controlled FPS gain/длительная стабильность не доказаны.

Neutral DOF/SSAO texture-work skips в `res/soc/gamedata/shaders/gl/dof.h`, `ssao.ps`, `common_samplers.h`; semantically neutral cases не снижают качество. У SSAO quality0 samples отключены. `glState.cpp` сокращает sampler extent checks, но первый/reset/force binding и очистка предыдущего tail сохраняются.

FPS unlock поддержан в лаунчере/Activity: checkbox добавляет `-unlock_fps`, Android high-refresh request. Меню имеет свой limiter. Loaded-game presented FPS превышал 60 на TabS9, refresh120Hz; наличие checkbox не гарантирует 120FPS или фиксированный OS refresh.

### 3.5. AI use-after-free

На taskspot crash подтверждено уничтоженное smart terrain task: boar `esc_boar_weak_0006`, owner862, stage graph_execute, taskgeneration3515. Записаны create→initialize→destroy→invalid_use. Дальше intentional diagnostic FATAL/SIGTRAP5 до invalid dereference. Предыдущий SIGSEGV согласуется, но его exactlifetime не записан.

Исправление удерживает **точный Lua userdata через registry root**, а не просто shared/raw C++ pointer. `alife_smart_terrain_task_owner.h`, `alife_smart_terrain_task*`, `monster_state_smart_terrain_task*`, native immediate callers в stalker/alife brain и server entities. Releases при finalize/failed start/reinit и до Lua VM reset; не отменять эту логику случайным refactor/copy. Старый owner/audit host tests8/20 cases используют mocksregistry, не realfullGC.

Taskfix наблюдалась~12m16s с 16 repeatedgraph bindings прежнего боара без вылета; позже audit coverage стал частичным из-за unknown tasks/capacity. ForcedGC в полном движке не проводился; не считать это общей гарантией стабильности.

### 3.6. FXAA и Android resume

SoC/GLES `r2_aa on` выбирает мобильный FXAA12 в существующем combine pass (не дополнительный framebuffer). `gl_rendertarget_phase_combine.cpp`, `combine_2_aa.ps`, `combine_2_naa.ps`, `fxaa.ps`. RGB-derived perceptual luma, highp s_image, reciprocal size из textureSize — не подменять physical screen size при render_scale. AA-off/menu и desktop path сохраняются. Пользователь подтвердил более ровные дальние края/оружие, но сильная рябь листвы осталась.

`AndroidAppLifecycle.h`, `x_ray.cpp`, `device.cpp`, `frame_trace.h`: SDL APP events извлекаются/acknowledge до оконных resize/reset/frame work; известный background блокирует GL/loadscreen frames, ложный watchdog stall подавлен, heartbeat rearm на foreground. Для SDL2.30.12 DIDENTERBACKGROUND ack нужен, чтобы SDL отпустила/backup-restored сохранённый EGL context. На TabS9 два background cycles32.084s/6.637s, тот же PID/context, полный кадр/звук/управление, no new EGL/reset/STALL. Пользователь подтвердил обычный возврат. **Настоящее уничтожение/замена GL context со всеми GPU resource rebuild не реализовано.** Host lifecycle16scenarios/507assertions моделирует JNI/EGL/events, не реальный драйвер.

## 4. Установленные APK, фактические runtime данные и ресурсы лаунчера

### APK

| Версия | APK в W/outputs | SHA256 | Статус |
|---|---|---|---|
| spotfix | openxray-soc-spotfix-arm64.apk | c4719acb5f31f2ae7ed15e0c080170ad7f2212a2089a5c03368060fe67ca88f9 | Проверенный откат с тенью фонарика |
| nightresume | openxray-soc-nightresume-arm64.apk | 4a6a0107d9df05fb06d14083d40dc9a2e559f62168e5932e531e73305c97dfd3 | Проверены loaded save/AA/обычный resume/uniformbatch |
| zoneskin | openxray-soc-zoneskin-arm64.apk | 565af17787cca69c1db69b0b88596c57b0b43f78552222073c93afafd437d7c7 | UIv1, старый сохранённый откат |
| zoneskin2 | openxray-soc-zoneskin2-arm64.apk | 3e8931c87e45488ce2ea92d4defb404144838d28f55304419ab66eef11c0f3b5 | Собрана, не устанавливалась |
| zoneskin3 | openxray-soc-zoneskin3-arm64.apk | b53bb3863f578c9ecf02ba98ba03b5f1485ef4e7295fcba419118d34c61f33db | Последняя установленная;30,943,911bytes;UI проверен |

В zoneskin1/2/3 все 27 native.so и старые gameassets побайтно равны nightresume; меняются Java/resources и ART profile metadata assets/dexopt. Поэтому новая торговля/quest/profile/AA UI из текущего дерева пока отсутствует в APKzoneskin3.

Nightresume runtime: без AA69–71presentedFPS, AAon66.6–67.5, batch+AA65.7–67.2. Различались ракурс, NPC, torch, нагрев, длительность — не доказательство точной цены AA или выигрыша оптимизации. Spotfix41–42 и более старые 20/40–50 также не контролируемый A/B. Не брать FPS меню как игровой FPS; рабочие отчёты относятся к загруженному quicksave.

Последняя установка zoneskin3 14Oct~20:26 по локальному machine stamp, UID10361, installedAPK SHA проверен после pull; user.ltx и 4savedfiles до/после одинаковые. После открытия dropdown/прокрутки конфигурация также побайтно неизменна. Source `outputs/zoneskin3-ui-runtime-report.json`. Lastcaptured user.ltx УЖЕ до установки содержал 1152x720, render_scale1, r2_aa off, rs_fps_limit501/menulimit501. Не приписывать 720p/AAoff новому лаунчеру. Старое state поле current_aa_enabled=true относится к предыдущей игровой сессии; актуальное дисковое `current_config_aa_enabled=false`. Metadata без даты/версии может быть stale.

### UI и точные ресурсы

Первый референс `C:/Users/User/Downloads/Постапокалиптический лаунчер OpenXRay.png`. Новый точный фон `Постапокалиптическая стена STALKER.png`,1448x1086; RGB пиксели сохранены lossless WebP. Current resource `android/app/src/main/res/drawable-nodpi/launcher_zone_background.webp`, SHA2eef856100d4a671c13ba99c0730bdd754b305e9d9889678bd55df47243c5353. Источник PNG в W/work/launcher-skin-assets/launcher_stalker_wall_original.png, SHAfd092e83e133807b7708d4181ee341c405d6a9a5ec817f2feb5824f654301246.

Последний panel reference `Ржавый постапокалиптический монитор с треснувшим стеклом-fotor-20261014193033.png`. Встроенный image_gen адаптировал два ассета: launcher_monitor_panel.webp и launcher_monitor_button.webp, оба 1536x1024 RGBA, alpha0..254, decodeRGBA точно равен PNG. PNG и полные prompts в W/work/launcher-skin-assets/; outputs/zoneskin3-image-prompts.json. Старый generated bronze plate/backdrop сохранены для истории, но текущий фон предоставлен пользователем, не сгенерирован заново. Не замещать текст настоящими нарисованными кнопками.

`LauncherBackdropView.java` рисует static cover image; `LauncherSkin.java` — presentation-only, shared decoded bitmaps, Oswald400/600, customnativeToggle, SeekBar и drawablePlate. Fullpanel border8fixed slices source sx8/192/1344/1528, sy42/176/782/952, side20/top14/bottom18dp; glassdraw coveraspecttop-right clipped, отдельное darkoverlay. Compactbutton sx32/136/1400/1504,sy172/288/720/832,side8/top9/bottom9dp. Винты/трещины не растягивать wholebitmap до tallpanel. Padding22/16/22/20dp; pressedoverlay только glass и Drawablealpha. Oswald variablefont неизменён, OFL1.1 включена в res/raw; legacySairaStencil сохранён, но body/headings Oswald. Системные Windows fonts не устанавливались.

Tablet columnweights hero.265/settings.44/actions.295; phoneheroGONE, settings.58/actions.42. ScrollViews независимые. В установленном zoneskin3 нижний Additionalarguments частично обрезан в initialscroll, но после scroll имеетполные 72physicalpx; все 45 resourceIDs, nofatal. Dropdown9resolutionchoices читаются; nooptionselected во времяпроверки. Startup/Newgame/Reportbuttons в этом UI цикле не нажимались. Screenshots W/outputs/zoneskin3-tablet-launcher.png и zoneskin3-tablet-settings-bottom.png. Пользователь одобрил дизайн.

## 5. Новая ночная работа — вошла в nightpolish APK, игровые проверки ожидаются

### 5.1. Торговля: уточнённая проблема и выключенная диагностика

Изначально пользователь видел свои вещи, но пустую сторону продавца у Сидоровича/NPC; луттрупов был виден. Позже **торговля у Сидоровича заработала на прежней zoneskin3 без нашегообновления**; другие NPC поканепроверены. Не заявлять, что торговля исправлена нашимкодом.

Offline read-only анализ старыхкопий saves через LZO genericSpawn/chunk2 обнаружил уСидоровича 35/30parenteditems, пищу/медикаменты/PDA. Это не доказывает onlineplacement/CanTrade/actualsellconfig. BasegamearchiveUI/ltx небыликопированы; actualui_style_default не равно overlayCS-styletrade.xml. Causefilter/onlineUI не подтверждён. GenericCInventoryItem::CanTrade = ownerpolicy && FCanTrade && !IsQuestItem; corpse AddAvailableItems(false) bypassesfilter. Нельзя «чинить» выдачей stock/bypassquestrestrictions.

Реализован **opt-in -trade_audit**, defaultoff. Файлы `src/xrGame/trade_audit.h`, `inventory_item.cpp::CanTrade`, `trade_parameters.h/inline.h::audit_sell_restriction`, `ui/UIActorMenuTrade.cpp::InitPartnerInventoryContents`. Не повторяет AllowItemToTrade evaluation;8snapshots×32itemrows, RAII/threadlocalowner guard, summaryraw/ruck/belt/slots/useful/evaluated/cantrade/cellcounts/gridabspos/size/visibility. `sell_restriction0none/1local/2global`, localprecedence. tested0 не означает falsepolicy — item небыл visited. Shareditemcontainers не меняются.

Host combinatorial/parsing/bounds/owner/nested/exception tests и actualARM64syntax3TUs passed. Evidence W/work/nighttrade-tests/{README.md,results.json,android-syntax-results.json,save-stock-evidence.json}; сохранятьоригинальные.sav неизменными. Diagnosticprofile нового launcher восстанавливает -trade_audit толькоесли он ужеесть customargs;newdefaults не включает.

### 5.2. Циклические квесты — доказанная исправленная ошибка

Пользователь: уСидоровича bringMedusa, artefact уже врюкзаке; box→pick тоженепомог. УШустрого findmodifiedjacket, взялпосле acceptance, тоженеобновляется; dialogue только refuse. Сценарии точно записаны дляпроверкипослеобновления.

Source defect: `SGameTaskObjective::CheckFunctions` вызывал SoC Lua только `(task_id)`, хотя неизменённый shipped `task_manager.script::CRandomTask:task_complete(p1,p2)` требует objectiveindex. Ветка p2==1 possession неработала, statuscompleted неустанавливался, have_completed_job/rewarddialog закрыт. Сюжет root/item/return индексы 0/1/2 важны; нельзя обойти conditions иливыдатьнаградувручную.

Исправлен ТОЛЬКО `src/xrGame/GameTask.cpp`:

- `CheckFunctions`: SoC(task_id,m_idx), другие gamesIDonly.
- `CallAllFuncs`: аналогично SoCactionfunctors с index.
- `CGameTask::ChangeStateCallback`: SoC всегда `(task,objective,state)`, включая root-only; latergames сохраняют прежний rootcallbacksignature.

Constructor CGameTask наследует SGameTaskObjective(this,ROOT_TASK_OBJECTIVE), поэтому rootparent валиден. Предыдущие taskmap/compatibilitypatches сохранены. Saveformat/taskdefinitions/scripts/rewards не менялись. Ужеактивный quest долженпереоценитьсяпосле load подисправленнымдвижком, если requireditem/queststate валидны; это прогноз по existingupdate, не deviceverification.

SHA GameTask.cpp0ec8fa5d958513eda37813744dc9b3a2c85fade0b9d0305a10feb1ed43a77f24. Taskmanager.scriptSHAcaf4d6576b4ca5b6cb04ba956dde6bdc6da93a192bd80cb8f2f7b4a131af6342; bind_stalker814ff73bd98b7ebeacfe065d572dcf086083acb4b9dbd81ba0e0552d9842cbe3.

Tests W/work/nightquest-tests: actual4productionmethods extracted/compiled;8native signatures,4callbackcontracts,8guards. ActualnativeCheckFunctions/CallAllFuncs call REAL Lua5.1 via localminilua Cbridge and execute unchanged task_manager.script. OldIDonly stall reproduced; correctedflow objective→turnin→reward→root→cyclicreset. 12exactitemworkflows acrossnative/Lua: af_medusa and novice_outfit_m1, eachpreowned/boxreacquired/afteracceptance. Wrong/absent/dropped/reacquireditem, deadgiver/refusal/duplicate rewards guarded. **af_medusa подтверждён вкопиях saves; novice_outfit_m1 — явнонеподтверждённый fixture длякуртки, taskltx недоступен.** Реальные inventory/NPC/clock/transfer stubbed; это не fullAndroidluabind/gameplay. ARM64syntaxGameTask.cpp/GameTask_script.cpppassed. README.md,final-summary.json,results.json,android-syntax-results.json,native.log,lua.log остаются work;portabletextcopies при checkpoint в docs/development-checkpoint/evidence.

### 5.3. Профили запуска, независимый FXAA и сохранностьконфига

Новыефайловыеизменения: `android/app/src/main/java/org/openxray/LauncherActivity.java`, `res/layout/activity_launcher.xml`, `item_resolution_spinner.xml`, `item_resolution_dropdown.xml`.

Normal всегда supply `-xclsx -gles_shadow_detach -gles_stream_upload -gles_uniform_batch` одинраз. Diagnostic adds `-gles_shadow_audit -gles_spot_shadow_audit -gles_spot_writer_audit -gles_perf`. Legacy optional `-ai_task_audit -gles_spot_shadow_dump -trade_audit` pausedNormal, restoredDiagnostic толькоеслибыли retainedcustomtext. Rejectrebind/orphan/sync воба modes (rebindnativeusesstrstr, поэтому embeddedtoken такжеподавлен). Unknowncustomargs/quotedarguments проходят;rawcustomtext сохраняетсяи backup`custom_args_before_profiles_v1`. Это неавтоматическоеудаление usertext;status/details explains actualeffectiveargs. Profilepersistkeys launch_profile,launch_profiles_v1_initialized. Newdefaults Normal. Читать source/tests дляточной migration, не «упрощать» потерейсохранённых customvalues.

Methods `buildProfileArguments`, `refreshFxaaFromConfig`, `readFxaaSetting`, `writeFxaaSetting`, `replaceSingleSetting`, `writeUserLtxLines`, `updateUserLtxSettingsIfChanged`, `selectionValueChanged`. FXAA readsactualper-mode user.ltx viaAtomicFile.openRead (backuprecovery), checkboxpendingwritesonlyonlaunch, respects explicitdirtychoice onpath/mode/resume. Expliciton survivespresetPotato. Atomicconfigwritefailure blockslaunch ratherthansilentlyclaimingapply. Устранён прежний bug: Nonepreset/resolution-onlywrite удалял managedgraphicskeys; теперь меняютсятолькопринадлежащие changedcontrolkeys, сохраняются unrelatedingamesettings и r2_aa. PresetwritesAA после_preset.

IDs `spinner_launch_profile`, `text_launch_profile_status`, `check_fxaa`. Ос wald/font/backdrop/monitorpaddingtopinset/weights unchanged. Пары Resolution+Backend, preset+FXAA, profile+args и horizontalgame-mode row сэкономили height. Offlineestimatebottomy1469vsviewport1534, clearance65px(~31dp), не devicefact. Alloriginal36controls preserved;newtotal будетбольшестарого 45resourceIDs, не assertoldcount дляновой UI.

W/work/nightlauncher-tests/run_source_checks.py:83 assertions исполняют extractedproductionJava (normal/diagnostic/migration/unknownquotedargs,AAroundtrip,presetorder,preserveNone/resolution/sun/lighting/difficulty, pergamefallback,IOfail). HostAtomicFile stub; actualAndroid atomicstorage/UI неизвестны. `check_tablet_fit.py`read-onlyoldhierarchy/fontmeasurement. Activity SHA65c727163227d795b764a78cea3ab99581761ae6456eb1e41f0168376281f352; XML2203244088f871a1b41818ecbd5cc76a3d3767c93fe28fd1801069afd1d8cf15. Source stable, nofullAPKbuild.

## 6. Рябьлиствы: исследования, отклонённыеварианты и незавершённыйплан

Foliage/detail usehardalphadiscard~200/255; spatialFXAA неможетгарантировать temporalsubpixelcoverage. CurrenteffectiveMobile constants subpix0.35,relative0.166,darkfloor0.0625. Legacy r2_aa_break/weight/kernel не используются mobilealgorithm. GLESr2_tf_mipbias no-op (unsupporteddesktopSamplerLODstateguards); texture_lod loader загружаетвсемипы/base0, меняетлишь estimatedret_msize, неэффективный baseLOD. AF реаленприменяется shaderstatecompile,restart; trilinearmips есть. Не обещатьреальноекачествоотнеработающих keys.

В subpixeltests actualpreprocessedFXAA12 viahostfloat32/bilinearclamp, reference16x16coverage; 0.75 даётлистве~2%motiongain при 13%worsespatial, stems~4%motion при 33%worsecoverage — **ОТКЛОНЁН**. 0.50 даёт leaf~0.4%spatial/~1.1%motion, stemsmixed/+5.6%motion с 2.5%spatialloss и brightcaseworse2.6%motion. Только laterdeviceA/Bcandidate, не default. 96GLSLES310/320glslcfrontendcases,254pairedcomparisons,31invariantspercandidate; readsidentical. FrontendOpenGLSPIR-V не ES300 драйвер/devicecheck.109alpha/shadowhashesunchanged.

Лучший newhypothesis: lowerdarkfloor0.03125, keep0.35/.166. Текущий 0.0625 пропускаетвсе controlledlow-lightleafedges.192pairedsyntheticframes: leafcoverageerror-25.2%/motionerror-8.2%, stems-37.4%/-5.1%, building-56.8%/-10%; bright/flatdark/faintnoise/rampcontrolsunchanged. **Extra hostreads** leaf+23.5%,densestems+74.5%,building+3.3% — не FPS. ApproxSaveddisplayframecrops: bushes+34%reads,shadowgrass+41%,canopy+28%,wood+17%,wall+4%; inputpostprocessed/resampled не originals_image инет qualitygroundtruth.

Решение: сохранить Fastdefault;possibleopt-inFineedgesexplicitquality/perfcost. **Не реализовано.** Следующийагентпослеревьюможетдобавить int`ps_r2_aa_detail_mode=0`, command`r2_aa_detail_mode0/1` в `src/Layers/xrRender/xrRender_console.cpp/.h`; AA-onlyuploadfloat4`e_fxaa_quality.x` .0625/.03125 в `gl_rendertarget_phase_combine.cpp`; OXR_MOBILE_FXAA uniformargument в `res/soc/gamedata/shaders/gl/combine_2_naa.ps`. Не менять fxaa.ps/subpix/searchpasses/alphashadowcutoffs. Лаунчеру читать/писать selectorFast/Fineedges какнезависимый persistedconfigkey после preset, preserveunrelatedsettings. Mode0 долженвоспроизводитьнынешний output, missingmode=>Fast. Новый uniform/console/native linkage ещёнет/непроверен. Затем actualsame-sceneFast/Fineedgesdeviceframe-time+motion при user.

Полныйплан/evidence `W/work/nightfoliage-tests/foliage-dark-cutoff-handoff.md`, `foliage-audit.md`, `dark-cutoff-results.json`, subpixel-results*,polarity-results,shader-results*,preservation-results. В checkpointcopy включены text/source/report, не тысячи generatedfloat/image/intermediatefiles. Originals сохраняются work.

## 7. Сборка: точные инструменты, команды и ловушки после commit

### Локальныеинструменты

- Python `C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`.
- Git `C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/git/cmd/git.exe`.
- JDK `W/work/toolchain/jdk-17.0.20.1+1`.
- SDK `W/work/android-sdk`; build-tools34.0.0,platform34,NDK27.0.12077973,CMake3.22.1.
- Gradlewrapper8.2, cache `W/work/gradle-cache`, Androiduserdir `W/work/android-user`.
- HostMinGW `C:/MinGW/bin/gcc.exe`,g++.exe.
- Test signingkey `W/work/android-user/debug.keystore` **приватныйлокальныйфайл, НЕ Git/sourceZIP/checkpoint**. Нужендляобновления org.openxray.dev ссохранениемданных. НовыйключдругогоПКнеобновит APK сэтойподписью; backupkeys поотдельномупоручению, не publish.

Из W PowerShell:

```powershell
Set-Location 'C:/Users/User/Documents/Codex/2026-10-11/gjvyb'
& .\work\build.ps1 -Variant nightpolish *> .\work\build-nightpolish.log
```

**Команда выполнена успешно: BUILD SUCCESSFUL in 2m 24s, 47 tasks (26 executed, 21 up-to-date).** `build.ps1` acceptsanyVariant;устанавливаеттолько processenvJAVA_HOME,GRADLE_USER_HOME,ANDROID_USER_HOME,PATH/git config,GIT_CONFIG_GLOBALlocal,CMakeparallel4; вызывает gradlewassembleReleasewith`-POXRAY_TEST_BUILD=true -POXRAY_TEST_VARIANT=nightpolish -POXRAY_KEYSTORE=...` --no-daemon --console=plain --max-workers4. APKoutputs `R/android/app/build/outputs/apk/release/app-release.apk`;symbols `R/android/app/build/outputs/native-debug-symbols/release/native-debug-symbols.zip`. Не overwrite готовый zoneskin3APK;copynewvariant.

На fresh ПК: JDK17/Git/MinGW/SDKpackagesabove, свой android/local.properties` sdk.dir=C:/Android/Sdk`; knownfetchinternetrequired. initrecursive7submodulesoruseselfcontainedsourceexport; GameSpylocalcommitimport отдельно. RootCMakeusesNDKAndroidtoolchainandGLES viaGradle config. NoLinuxdualboot установленагентом; пользователемпланировалсяпозже, фактготовностинедоступен.

### Packaginghelpers

W/work/start_taskfix_build.py,package_streamupload.py,verify_candidate.py,install_candidate.py поддерживают taskfix,spotdepth,spotfix,nightwork,nightresume,zoneskin1/2/3 (somehelpers taskspot/streamvariants), **nightpolish ещёнеразрешёнв assert**. Для nightpolish вместо изменения старых helpers создан W/work/nightpolish_pipeline.py (prepare/verify); он сохраняет все tracked inputs, включая submodules, и строит cumulative patches от исходных баз. Старые helpers не использовать для nightpolish без отдельной адаптации. Не прогонять`record_zoneskin_prepared.py`для nightpolish — он assertallnativebytesidenticalnightresume, а xrGame изменён.

**Послеэтого Gitcommit нельзяполагатьсяна`git diff HEAD`как freeze или cumulativepatch**: рабочеедеревочистое, diff пуст. Starter нужно freeze через trackedsourceinventory/commit+sha, includeuntrackedwhenpresentandGameSpycommit;hashallrelevantinputs. Cumulativepatch считатьот base4223fa91...;GameSpy от 3e434803..., илииспользовать Gitcommits/bundles. Старые zone3buildinputs/patch/ZIP описывают pre-quest/pre-profilebaseline иНЕобновляютсяавтоматически.

Для newAPK обязательны Gradle/lintresult,sourceinputsunchangedduringbuild,verify_apk.py ZIPCRC/ELF64AArch64/27libs16KB,apksignerverifyv2,aaptdumpbadgingpackage/version,source/manifestspecifictests. EngineoldAI8/20/sourcehashtests и shaderbaselinecheckscope нельзяпереименовыватьв newruntime. Sourcebranchcodecommit/Gitmetadata можетменятьбинарные bytes дажебез shaderchanges; не assertglobal27byteidentical при questfix.

Примеринструментовпосле build (pathsfromW):

```powershell
& 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' .\work\verify_apk.py .\outputs\openxray-soc-nightpolish-arm64.apk
& '.\work\toolchain\jdk-17.0.20.1+1\bin\java.exe' -jar '.\work\android-sdk\build-tools\34.0.0\lib\apksigner.jar' verify --verbose --print-certs '.\outputs\openxray-soc-nightpolish-arm64.apk'
& '.\work\android-sdk\build-tools\34.0.0\aapt.exe' dump badging '.\outputs\openxray-soc-nightpolish-arm64.apk'
```

Hostnighttests originals (сначала readREADME; checkandroidneedscompile_commands/PCHfrombuildtree):

```powershell
& 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' .\work\nightquest-tests\extract_and_run.py
& 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' .\work\nightquest-tests\check_android.py
& 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' .\work\nightlauncher-tests\run_source_checks.py
& 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' .\work\nightlauncher-tests\check_tablet_fit.py
& 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' .\work\nighttrade-tests\extract_and_run.py
```

EvidencecopiesinsideGit `docs/development-checkpoint/evidence/` — для portableinspection;scriptsderiveoriginalW/worklayout, поэтомуне claim автоматическоевыполнение copiedscripts на freshclone безадаптации paths. Сохранены readme,Python/testsource/JSONsummaries,не exe/objects/buildcache/temps/gamefiles.

## 8. План и приоритеты продолжения

1. Прочитатьэтотдокумент+CONTINUE_PROMPT,gitlog/status,handoffjson;подтвердить currentfilehashes/agentfreeze. Ничегонестёрлось,workingcode непереписывалсяради docs.
2. **Сборка nightpolish уже готова. Первый практический шаг: после подключения пользователем планшета сохранить settings/saves, установить готовую APK с -r и проверить существующие задания, профили и FXAA.** ТорговляуСидораработаетпо user;tradeaudit для othersoffuntilneeded. Save/questfix созданнекак newquestorcheat,подготовить runtimeverificationexistingquests.
3. Продолжитьотдельныйопциональный Fineedges толькос Fastbaseline/default и testquality/perfcost. Можносначалаотдать stablequest/UIcandidate, затем rendererqualitycandidate;не смешиватьнепроверенный mode сверифицированнойквестовойпричиной.
4. Послевозвращения user и explicitdeviceavailability: убедиться appclosed,backupconfig/saves/installedAPK;install-rsamekey/UID/hashverify;реальный launcher AA/profile/presetroundtrip/fit;loadexistingquestMedusa/jacket,turninonce/reset/save-load; NPCtrade еслиповторитсяс-trade_audit. Gameplay значениянеизменятьскрыто.
5. Пункт 2 старогоплана (longstability/26sstall иповторные FPS проверки) толькопри user;standing/camp/boars/trading/transitionlogs. Ordinaryresume2cyclesverified но deepcontextdestructionpending.
6. FoliaFast/Fineedges actualsameposition/camera/resolution/settings/torch/thermal A/B,measuregamepresentedframesnotmenu. Небросатьслучайные cache/buffer/stateoptimizations врабочийрендер.
7. Vulkan/ANGLE послестабильнойконтрольной GLES сборкиифункциональных quests/trade/save. Отдельновендорить/собрать ANGLEEGL/GLESlibs,link/load/backendselection/probe/devicevalidate;не claim скорость/качествоавтоматическими. CurrentANGLEnotincluded;outputs/ANGLE-NEXT-STEPS.md историческийпланможетбыть stale.

## 9. ADB и данные устройства — только для следующего согласованного теста

ADB `C:/Users/User/Downloads/platform-tools-latest-windows/platform-tools/adb.exe`;serialR52X40ES4LE. В PowerShell пользовательправильнозапускает `.\adb`,не plainadb без PATH. GalaxyTabS9Android16/API36,Adreno740,driver0676.77.1Jan2026,display2560x1600landscape,density340/~1205x753dp,refresh120. S26Ultra фактические GPU/driver/buildcompat непроверены,неугадыватьпроцессорпообщимназваниям.

Gamebase `/storage/emulated/0/Download/socc/sas`,mode `/soc`,appdata `/soc/_appdata_`,engineLog`logs/openxray_u0_a361.log` зависит UID,нефиксироватькакуниверсальное. QuickSave `u0_a360 - quicksave`. AppUID10361/u0_a361. User pastedcrashlogs были Downloads/cruch.log и log.txt; screenrecordings2DCIMcapturedinwork/video*;нет liveclipboard доступапредположений.

Будущие commands пример (НЕвыполнятьпока user спит):

```powershell
& 'C:/Users/User/Downloads/platform-tools-latest-windows/platform-tools/adb.exe' devices -l
& 'C:/Users/User/Downloads/platform-tools-latest-windows/platform-tools/adb.exe' -s R52X40ES4LE shell pidof org.openxray.dev
& 'C:/Users/User/Downloads/platform-tools-latest-windows/platform-tools/adb.exe' -s R52X40ES4LE install -r .\outputs\openxray-soc-nightpolish-arm64.apk
```

Installhelperbacksupuser.ltx/savedgames,pullsinstalledbase.apkverifySHA,sameUID/savedfilehashes. Не deleteappdata/игру/сейвы/старую APK для«быстрого»install. Пока runningnativegame — не force-stop без usercoordination. РаньшеостанавливалиТОЛЬКО Launcher послепроверки foreground,для UIiteration. Черный screencap однаждыбыл tabletDozing/NotificationShade,не launchercrash:power/windowdiag+KEYCODE_WAKEUP224 различили. Не bypassPIN.

## 10. Неудачныеподходы, важные решения и границы знания

- `-gles_state_rebind`: forcedfullstatehelpedcorrectness butFPS~30vsuser40–50; дорого,нерабочий default. Sharedphysicalstatecache былболееузкимрешением. Не«ускорять»удалением binds/resetinvalidations без actualtransitiontests.
- `-gles_stream_orphan` unsynchronizedwrites: textures/HUDgeometry скакали,огромныетеневые/текстурныеполосы. Отменено;nativeflag намеренноне recognizesoldexperiment. `-gles_stream_sync` driver-managedcomparison не selectedworkingpath; оба pausedprofiles. DynamicCPUstreamupload выбранпослевизпроверок.
- Lowp/highp s_positionsamplehypothesis:headlessRGBA16F/32F0/80bitdifferences;неподтвердило primarycause. GLmatrixtransposeprobeGL_TRUEvalid/noerror;непереворачиватьматрицынаугад. Старый TSMhelperissue не active при newcascades.
- Изменение sun_near_border пользовательупоминалкакэмуляторный workaround;неподтверждённаяглавная native причина. Source/GPUattachment и opaqueprogrambugs былидоказанывместоэтого.
- Lightingquality/shadowcorrectness былаважнееискусственного FPS. Случайный«fastcache»вызвалхаос;refusedafteruserfeedback. Baselines/oldAPKs оставлены.
- Исходный UIstyle былдоработанотфотореференса кточнойстене и rustymonitor;шрифт/рамкиреальный AndroidUI,неплоский mockup. Передсменой bodyfontuser просилсначалафон/darkrust/widerlaunchblock. OswaldchosenwithCyrillic/OFL, noOSfontinstall.
- Subpixel0.75 непринятдажехоть compilepassed:qualitycoverageworse13–33%. 0.50mixednotdefault. Darkfloor ниже promising только opt-in из-за extra reads;не TAA/perfecttemporalAA.
- Полноесглаживаниелиствы,actualS26Ultra/Vulkanadvantage,causalFPSgain,realGC/fullAIstability,contextreplacementrecovery,точная jacketsection/basequestltx,остальные NPC торговля — **недоступно/непроверено**. Не придумыватьрезультаты.
- Новаяночнаяработанеизменяла device/windows/shutdown/SDKlicenses. Игрокотключилпланшет;tests наПКнезаменяютреальный save/gameplay.

## 11. Git, source export, перенос и сохранённые артефакты

Checkpoint сохраняетв Git рабочиеисходники/ресурсы/docs и lightweightevidence,неключи/SDK/logs/saves/buildtemps. GameSpy имеетотдельный localcommit выше; maincommitrecord в handoffjson+gitlog. Никакогопубличного push/PR небыло. Рекомендуемыепортативные bundles W/outputs/openxray-development-checkpoint.bundle и gamespy-development-checkpoint.bundle,создаютсятолькоизнаших newcommits с baseprerequisites,не privatebuildtools. Ихналичие/verification смотреть handoffjson.

**Ранееотданный GitHubsourceZIP УСТАРЕЛотносительноэтого checkpoint**: W/outputs/openxray-zoneskin3-full-source.zip47,854,119bytes,SHA68f13495f30ba1a072441262989c48f73a2a8eb422f6c3fd6b75d33d499d931a. Онполностьюсоответствует installedzoneskin3 доночного quest/UI.11289sourcefiles,7flatteneddependencies,all91oldbuildinputsidentical;ZIPCRC/hash/privatefilename/emptyReleaseGitHubtokenchecked. Export-onlyrootREADME/.gitignore+2nested.gitignore измененычтоб 290 ранее trackedsourcefiles неисчезлипри freshGitimport (HostBuildTools/buildvm,Makefile,etc); .gitmodulesomitted в flattenedexport. NotfreshcompiledfromZIP. Не использоватькакновый nightpolishsource.

Forkuploadguidance осталасьнезавершённой: пользовательспросилкакзалитьв fork,но interrupt переключилнат launchargs/ночь. Правильнаяоснова—clone своего fork и normalcommits систорией,не forcepush историиизнового gitinit. NeedactualforkURLandGameSpysubmodulepublishingstrategy;flatZIP для standaloneimport сампо cebe незаменяет gitlinks в fork. APK лучше GitHubReleases,source в Codetree. Currentoriginupstreamneverpush автоматически.

Сохранены outputs:APKs/symbols/buildlogs/cumulativepatches/manifests/SHA256SUMS; oldreports spotfix*,nightresume*,zoneskin3*. Workcontainsprivategame/runtimeproof,tests,probes,backupcopyconfigs/saves и toolchain. README-RU/project-state.json до checkpoint описывают installedAPK,не live с ourcenewfeatures; checkpointstate/report+этотдокументприоритетнееих stalesourcefacts.

Машинныйполныйсписок changedsources/hashes,подмодулей,текстовых tests—`docs/development-checkpoint/source-inventory.json` и evidence/. Ихсозданиетолькодокументируетфайлы,неподменяет build/runtimeverification. Для continuation наэтомПКдостаточносохранитьвесь W;длядругогоПКпередать Gitcommit/bundles+docs,принадлежащиепользователю gamefiles отдельно,настроить SDK/ключлокально. Приватный testkey никогдане publish.
