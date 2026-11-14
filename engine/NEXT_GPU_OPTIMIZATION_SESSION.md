# Продолжение GPU-оптимизации OpenXRay Android

## Сначала прочитать

- `CORDON_GPU_PERFORMANCE_REPORT.md` в корне этого репозитория.
- `C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/cordon-gpu-20261022/session-state.json`.
- В той же evidence-папке: `implementation-evidence.md`, `final-source-changes.json`, `FINAL_SOURCE_REVIEW.md`.
- `device-validation/DEVICE_VALIDATION_REPORT.md` и `results.json`, если уже созданы: они содержат самые поздние проверки устройства. Ранний checkpoint не заменяет их.

**Сессия завершена. Обычная cordonperf1 установлена и проверена в загруженной игре на Native GLES и настоящем ANGLE Vulkan.** Финальные normal FHD SF: Native45.890/51.072, ANGLE54.065/65.340 FPS (обычное время/погода, не matched API comparison). AA on/off и сцена у костра проверены; background recovery с оговорками helper описан в device-validation/results.json. Точный original11703/config hash30fe… и четыре исходных save hash восстановлены. Backendangle_vulkan, extraargsпусто, 26protectedUIпараметров неизменны. Приложение закрыто, экранDozing/OFF, stayon0. ADB свободен, живых фоновых команд нет. Пользователь попросил выключить Windows после сохранения итогов.

## Задача и ограничения

Цель пользователя — повысить производительность тяжёлой сцены Кордона при настоящих 1920×1080, сохраняя графику, стабильность и оба пути: Native GLES и GLES→ANGLE→Vulkan. Нативный Vulkan сейчас не реализуется. Разрешены ADB, сборки, диагностические APK, консоль, безопасные изменения и субагенты. Оригинальные сохранения, пользовательские данные и предыдущие APK защищать. Git/push/упаковку релиза пользователь просил отложить.

**Вне измерений гасить экран и приостанавливать/закрывать игру.** Пользователь отдельно попросил не оставлять планшет нагреваться во время анализа/сборки. Предпочтительно закрывать игру после серии. Исходное Android stay_on_while_plugged_in=0; временное USB stay-on разрешено только в тестовом блоке с finally, восстанавливающим 0 и экран OFF.

**Консоль закрывать ESC (Android111) и проверять скриншотом.** Не измерять меню, загрузку или консоль. После wake дать SDL минимум2сек на foreground/focus. После `load` нельзя посылать ESC во время staged loading: это открывало меню и провоцировало обнаруженный null HOM-task crash старой сборки. Helper откладывает этот ESC до визуальной проверки. Если load сам скрыл консоль, поздний ESC откроет меню; проверить и ещё одним ESC вернуться в мир.

Не уменьшать разрешение, плотность/дальность травы, качество теней, освещение, AA или эффекты ради результата. Временные отключения допустимы только для диагностики с немедленным восстановлением.

## Среда

Workspace: `C:/Users/User/Documents/Codex/2026-10-11/gjvyb`.
Репозиторий: `work/openxray-build` (пути исходников ниже относительно него).
Evidence: `work/cordon-gpu-20261022` (D).
Windows11/PowerShell, WSL2 Ubuntu24.04 доступна для host ASan/UBSan.
Python: `C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`.
ADB: `work/android-sdk/platform-tools/adb.exe`.
Serial: `R52X40ES4LE`, Tab S9 SM-X710, package `org.openxray.dev`, UID10361.
Game: `/sdcard/Download/socc/sas/soc`; data: `.../_appdata_`; log: `.../logs/openxray_u0_a361.log`.

Git HEAD/main: `28e6ff87203f0b93bf1935c9692b282d605b7072`. Рабочее дерево содержит предыдущие важные незакоммиченные изменения: **не сбрасывать на HEAD**. До этой сессии сделан полный `D/source-baseline.zip` (11715файлов,54008047байт), хэши в baseline-manifest.json. Это реальное рабочее дерево, включая необходимые ignored JNI-библиотеки, без ключей и build caches.

## Что изменено

1. **Уникальная регистрация уровня.** Шесть локальных Remove-before-Add в IGame_Level::Load, MainMenu::Activate(false), MainMenu::Screenshot. Исправляет наблюдавшиеся два World render за кадр; глобальный MessageRegistry не менялся. Реальные Release/DEBUG fixtures, mutation tests, ASan/UBSan, NDK пройдены. Probe2+ дают World1 после загрузки.
2. **Пакетирование uniforms травы.** `glDetailManager_VS.cpp` и новый `glDetailUniformBatch.h`. Под существующим `-gles_uniform_batch` прежние четыре vec4 на экземпляр собираются в массив и передаются одним bulk setter перед прежним draw. До61экземпляра, хвост передаёт только заполненный префикс. Данные, матрицы, цвет, порядок, шейдеры, геометрия, плотность и draws сохранены. Неизвестные layouts fallback scalar. 854сценария для каждого из трёх вариантов флага, побайтовое сравнение uniform state при каждом draw, ASan/UBSan и full NDK TU checks.
3. **Меню/HOM.** `r2.cpp::OnCameraUpdated` и `r2_R_calculate.cpp::Calculate` согласованы с существующим Render predicate `CanSkipSceneRendering()`. Обычное меню пропускает HOM dispatch/wait; gamesave screenshot получает нужную подготовку. Исправляет воспроизведённый crash17:14:25: Wait(*null ProcessHOMTask), address0x78. TaskManager не менялся. bReady guard был бы недостаточен. Реальные функции, 30production-сценариев, sanitizer и NDK пройдены.
4. **Сортировка.** `r__dsgraph_render.cpp::cmp_pass` теперь чистый строгий SSA>, без pairwise SPass::equal guard. Старый>= воспроизводил ASan heap overflow на64canonicalpass с равным SSA. Простая замена>=на> с сохранением equal-guard недостаточна для транзитивности. GL/DX интернируют equal passes. 4000sort rounds и NDK пройдены. Не доказано, что это было причиной конкретного игрового вылета; FPS-прирост от сортировки не заявляется.
5. **Изолированная диагностика.** Новый GLGpuProfiler и compile-guarded hooks; обычная APK полностью исключает их. Probe1 cap32 оказался недостаточен для56events и отбрасывал игровые GPU samples. Probe2 исправил cap128, stride32, monotonic frame ID с отдельным device_frame. Queries неблокирующие; disjoint/context reset обработаны.

Итого16production/build файлов: D/final-source-changes.json, общий D/final-changes.patch. Исходники заморожены до сборки в outputs/angle-work/cordonperf1-inputs.json. Шейдеры, текстуры, RT formats, AI/quests, UI и управление не менялись. Предыдущие FS mapping/Skeleton/task fixes сохранены и проверены verifier.

## Сборки и установка

Обычная: `& .\work\build.ps1 -Variant cordonperf1`.
Диагностическая: `& .\work\build_gpu_diag.ps1 -Variant cordonprobe4`.
Verifier: `python work/verify_angleaudit_apk.py --variant cordonperf1 --expect-angle present` (использовать указанный выше Python).
Wrapper задаёт локальные JDK17/SDK/NDK/Git/Gradle directories, системные настройки Windows менять не нужно.

Критичная особенность: **наличие** Gradle property OXRAY_GPU_PROFILE включает диагностику. Даже `-POXRAY_GPU_PROFILE=false` включает ON. В normal property должно отсутствовать; Gradle явно передаёт CMakeOFF и сбрасывает старый ON cache.

Финальная APK: `outputs/openxray-soc-cordonperf1-arm64.apk`, version1.6.02-cordonperf1,33697196байт, SHA256:
`22f0ec05d00ee25ad9f62a4aafcbddc99d92eee98ca92e8f4a93d0d3e5738df2`.
Profiler strings/API/schema отсутствуют в renderer, CMakeOFF,29ARM64libs,16K alignment, сертификат и C++runtime проверены. D/normal-build-proof.json.

Установлена21:10:59 через `work/install_cordonperf1.py`, UID10361 и hash подтверждены,10save files и2внешних конфига не изменены установкой. Бэкап: `work/angle-runtime/install-cordonperf1-20261022-211049`. Installer ожидает неактивную probe4 и не должен повторно запускаться, если final уже установлен. Сам игру не открывает. Runtime проверяется отдельно.

Старые APK для воспроизведения:

- perf20261022:7e12dac3fb52cc081ee3dc49e49641e58acc03725def2cbd2a01fc03e55fa6f3 — первоначальная accepted сборка, без измерительной диагностики.
- probe1:a865add37bfb88202774f3a8c8ccf3265ed16d63cfef56c3c064f814f0775fdf — cap32, игровые GPU данные недостоверно отсутствуют.
- probe2:beeb837f624b964b14eebe64c36edb1ba8d7472c522963180c5db44ad77fec40 — registration+profiler128, без новой травы/HOMfix.
- probe3:4fce93aef0247a3c4c6c81520faba18e42f4e0f02c6746dc5a5818579489808a — собран, root не устанавливал.
- probe4:5153097abf94afe4700a741afe0713a3d2fa21903f99d7435be8fbc60fa851df — diagnostic с травой и HOMfix, без последнего comparator fix.

## Сейвы, часы и настройки

Оригиналы: D/savedgames-original, четыре файла quicksave (u0_a360 Oct8 иu0_a361 Oct19, sav+dds). Не перезаписывать.
`cg22ref` — собственный сейв позиции, host D/cg22ref.sav.
`cg22benchp4` — отдельный диагностический сейв для сравнения. Диагностические extras не удалять wildcard-операциями.
Камера: [-55.414333,-2.253043,-64.284225,0.928497,-0.231284,0.290519]. Первые3позиция, последние3направление.

Исходный пользовательский «1080p» реально1728×1080. Контрольные тесты —1920×1080, проверять по engine/profiler, не по названию UI. render_scale1, details49/density0.62, shadow2048, исходноеr2_aa off.

**SAV хранит al_time_factor, но net_import может заменить его на базовый10.** Поэтому первый purported frozen A/B оказался несопоставимым по солнцу. Правильный эксперимент: при закрытой игре изменить только time_factor_single на0.001 в тестовой копииuser.ltx, затем load того жеcg22benchp4, после полного load запросить оба фактора — должны быть0.001. `D/frozen_clock_config.py` делает только offline копию одной строки, сохраняет исходные байты и SHA. ADBpull/push для конфигов; shell cat на Windows может добавитьCR и испортить байтовое сравнение. Для обычной игры вернуть10/исходную конфигурацию.

Финальное восстановление: **точные** D/initial-124014/user.ltx,11703байта,SHA30fe62d9dc9cd06b8225c61e6e2ce7d5d54ea743a6dde160cca7674e65074a89; исходный launcher backendangle_vulkan иextraargsпусто. После закрытия игры проверить обратнымpull и4originalsavehash. 11661-байтная копия использовалась только в тестах. Финальное восстановление11703подтверждено в device-validation/final-restoration-225222-044586/result.json.

Normal автоаргументы: -xclsx -gles_shadow_detach -gles_stream_upload -gles_uniform_batch плюсsoc/rgl/nointro/unlockfps. Не включать отвергнутые -gles_stream_orphan/-gles_stream_sync/-gles_state_rebind. Normal launcher вырезает -gles_perf из customargs; не переключать Diagnostic ради него, это добавляет тяжёлые shadow audits.

## Подтверждённый результат и ограничения

Правильная Native пара, оба фактора0.001, один сейв/камера/1920×1080/исходные эффекты, thermal0:

- probe2 SF40.954/40.299FPS → probe4 45.939/45.112FPS.
- По сумме кадров/длительностей40.627→45.526FPS (+12.06%). Среднийpresent interval24.614→21.965мс (-2.649мс/-10.76%).
- CPUdetails mean4.435→2.367мс; render thread CPU median16.993→15.073мс.
- Одинаковые26040detail submissions и2790sun subset; draws4689.6→4708.7 (+0.4%, динамические объекты).
- Thermalafter0,AP43.4→43.6°C. GPUtotal18.329→17.404мс в отдельных окнах.
- P95/P99present≈33.333мс почти не изменились;1%low улучшился скромно. Не обещать исправление всех редких фризов.

D/frozen-native-comparison.json, frozen2-native-window.json/.md, frozen4-native-window.json/.md содержат данные. Это два30сек окна каждой версии, не длительный рандомизированныйABAB; GPUокна не абсолютно синхронизированы с SF. Статический roadROI MAE0.680/255 и визуальный осмотр не нашли ухудшения геометрии/теней; это не полная временная/попиксельная идентичность.

Ранние серии mixed weather/thermal нельзя использовать как A/B. В частности Native4sun4.3мс против старогоNative2sun0.001мс — несопоставимы. Ошибочно названный capture probe2-native-1920-r1...153235 был у Сидоровича, SCENE_CORRECTION.json исключает его изCordoncomparison.

ANGLEprobe4 успешно отображает ту же сцену, SF42.793/42.195FPS. GPUtimestamps48bits: total18.136мс, sun5.307,geometry5.217,local4.747,combine2.210. Отдельного strictANGLEbefore/after нет. Native timestamp detail scope≈0.05мс противANGLE≈3.165мс нельзя напрямую сравнивать как преимущество драйвера: границы tile/renderpass execution могут различаться.

GPUstages inclusive и пересекаются, их не суммировать. Detailcounts=submissions, logicalpasses≠аппаратныеVkpasses. CPUwall≠threadCPU≠GPU≠SFpresent. MissingGPUrow≠0. Неполный хвост/rawloading сохранять, не подменять результаты. D/telemetry/gpu_window.py выбирает реальные непрерывные world/camera/res windows; gpu_crop.py явно документирует исключение неоднозначных loading-prefix старыхframeIDs.

Предыдущая perf20261022 субъективно стала стабильнее по отзыву пользователя; сопоставимого gameplay baseline angleexposure2 нет, точный выигрыш девяти прежних исправлений не доказан. Новая12% цифра относится к описанной паре, не ко всей истории разработки.

## С чего продолжить следующую сессию

1. Прочитать CORDON_GPU_PERFORMANCE_REPORT.md и D/device-validation/results.json. Последний итог важнее ранних pending-чекпоинтов. На устройстве уже обычнаяcordonperf1; не переустанавливать probe2/probe4 без причины и нового сохранения конфигов.
2. Пользовательские настройки восстановлены:1728×1080,AAoff,time_factor_single10. Это исходный выбор пользователя. Для нового FHD A/B отдельно сохранить текущие настройки и проверять настоящийFBO1920×1080. Не оставлять уменьшенный факторвремени или profiler после измерений.
3. При новых тестах единолично владетьADB. Root завершил прежнюю серию; никакой субагент больше не управляет устройством. Вне коротких тестовых блоков закрывать игру и гасить экран, stayon0.
4. Продолжить с GPU geometry/sun/local lights: большие стоимости подтверждены, конкретныйshader/bandwidth bottleneck ещё нет. Уточнить scopes и аппаратный trace, потом только проверяемый кандидат. Контрольный выигрышGrass уже доказан, не переписывать пакетирование ради количества изменений. Не снижать качество и не обещать60FPS.
5. Следующие независимые регрессии: разбитиеящиков/физика/трупы, более длинныйNative/ANGLEA/B/A/B,HDматрица, настоящееcontextloss. Квесты/торговлю/AI этойсессией полноценно не тестировали.

## Особенность финального lifecycle helper

HOMEсработал. Попыткавернуть MAIN/LAUNCHER intent с NEW_TASK|RESET_TASK_IF_NEEDED создала вторуюLauncherActivity, посколькустарыйtaskимел инойbaseintent. Диалог«processdied»оказалсяложным:старыйOpenXRayActivityбылSTOPPEDвтомжеPID22339. SKIP+BACKвернулиживоймирснормальнымизображением. См.final-return-223128-979257/activities-after.txt иfinal-recover-223523-635105/world-after-return.png. Это не nativecrash; следующийагентне должен автоматическиперезапускатьилиоткатыватьдвижокиз-заэтогодиалога. Дляобычноготеставозврата использоватьсуществующуюкарточкуRecents,проверятьреальнуюactivityиPID,неодинPID.

Возможнаяотдельнаязадача: LauncherActivity::offerCrashReportIfPending / CrashReporter::checkPendingCrash предполагают,чтоsessionmarkerозначаетсмертьпроцесса; при искусственном повторномсозданииLauncher надживойигройэтоневерно. Productionкодне менялся,обычноеповедениеиконкине исследовано. В helper исправлено только ошибочноеимяactivity: org.openxray.dev/org.openxray.LauncherActivity (applicationId и Java namespace различаются).

AA smoke: r2_aa on затемoff, обаexecutionconfirmed, ESCзакрываетконсоль,aa-on.pngпросмотрен. Это проверкаработоспособности,не новаяразработкаAAинеоценкаегостоимости.
Другаясцена: owned cg22smokep1.sav — точнаякопияoriginala360,строение/костёр/NPCнаКордоне. Оригинальныечетыреsav/ddsнеперезаписаны. Дополнительныеcg22*оставлены,неудалятьwildcard.

## Готовая инструкция новому агенту

Продолжи GPU-оптимизацию OpenXRay Android из C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build. Сначала прочитай этот файл, CORDON_GPU_PERFORMANCE_REPORT.md и work/cordon-gpu-20261022/device-validation/results.json. Обычная cordonperf1 уже установлена, оба backend проверены; источник содержит прежние незакоммиченные изменения, не сбрасывайGit. Измеренный Native FHD выигрыш пакетирования травы около12%, P95/P99почти прежние. Следующий фокус — узкие измерения geometry/sun/local lights и отдельные физические фризы, без снижениякачества. Перед новой серией сохрани текущие настройки/сейвы, соблюдай ESCдляконсоли и screenOFF/gamepausedвнеизмерений. Не считай различныепогоду/камеру или normalAPIruns строгимA/B. Не реализуй NativeVulkan и не меняйGit/push без отдельногопоручения. Не выключайПКвследующей сессии автоматически: запроснавыключение относилсяктекущему завершению; прочитай shutdown-result.json.
