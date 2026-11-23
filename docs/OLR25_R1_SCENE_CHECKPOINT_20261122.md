# OLR R1 GLES — контрольная точка 22.11.2026

Полноценный независимый R1 **не завершён**. Выполнен следующий проверенный этап:
небо/облака/согласование тумана, native trees, LOD и detail instances.
Модели и специальные материалы продолжают требовать R2. Направленная просадка
воспроизведена и остаётся нерешённой. Соответствие ПК по одинаковому кадру не доказано.

Рабочая копия: `R:/project/OLR-R1-parity`, ветка
`feature/olr-r1-gles-parity-20261121`, [draft PR #8](https://github.com/thithertom39/OLR-ANDROID/pull/8).
Исходные проекты и main не изменялись. Сторонние DDS, уровни и игровые архивы
не добавлялись в Git. 18 архивов на планшет повторно не передавались.

## Реализовано

- Отдельный `-olr_r1_scene`, добавляемый к трём прежним native-флагам.
  Native от 21.11 сохраняет прежнее поведение без этого нового аргумента.
- Небо: явный оригинальный множитель 2 из локального `r1/sky2.ps`, согласованный
  с половинным sky_color старого weather loader. USE_VTF больше не вносит
  зависимость нового режима от содержимого R2 tone-map texture.
- Облака: два оригинальных слоя, сумма RGBA, текстурная alpha и исходный
  CLOUD_FADE, вместо четырёх R2-выборок и dome fade.
- Native fog: передача необрезанного vertex fog factor и ограничение после
  интерполяции. POINT/SPOT сохраняют оригинальное ограничение fog в вершинах.
- Оставшиеся deferred-материалы в новом режиме используют тот же world-space
  fog_plane. Удалено дополнительное R2-смешивание fog² с небом. Для временного
  G-buffer position sampler в этом режиме требуется highp.
- Trees/bushes: исходные world transform, wind/rigidity, распаковка c_scale/c_bias,
  wrapped hemisphere, OLR flyer sun, ambient в CPU c_bias, base/detail, HQ/LQ,
  alpha test и blending. POINT/SPOT используют ту же деформацию и lookup lighting.
  Native aux elements выводят эти поверхности из основного G-buffer.
  Финальное исправление `54c719d` выбирает HQ/LQ по расстоянию: одинаковые R2
  элементы могут иметь один указатель после interning. На планшете подтверждены
  `shared_r2_hq_lq=1` для сосен и нескольких лиственных материалов. Сравнение
  указателей ошибочно выбирало им неподвижный LQ даже вблизи; оно заменено.
- LOD: исходные RGB/hemi/sun, коэффициенты 1.55 и 0.7, alpha нормалей `_nm`,
  отдельные depth/write и alpha-blend проходы с reference 200/8. Оба выполняются
  в forward, вместо прежнего deferred LOD.
- Detail instances: исходный CPU цвет ambient + hemi*h + 0.5*sun*s, still/wave,
  исходная fake-lighting модуляция ветром, fog, alpha test и отсутствие culling.
  Native детали пропускают основной deferred draw и рисуются в LDR forward.
  Неперенесённые детали сохраняют fallback. R2 shadow rendering деталей сохранён
  для неперенесённых зависимых материалов.
- Сохранены ранее проверенные lmap/vertex и оригинальные POINT/SPOT, portal
  subspace, bStatic exclusion, dedup, clip, RGBA8/ONE+ONE и обход HDR/exposure.

Формулы взяты из доступных OLR `R:/project/unpack/all/shaders/r1` и публичного
[R1 DetailManager_VS.cpp](https://github.com/OpenXRay/xray/blob/master/trunk/xr_3da/xrRender/DetailManager_VS.cpp),
[общих bindings](https://github.com/OpenXRay/xray/blob/master/trunk/xr_3da/Blender_Recorder_StandartBinding.cpp).
Референс не объявляется идентичным закрытой модифицированной PC DLL OLR.
Предыдущая подробная привязка lightmaps/lookup остаётся в
`OLR25_R1_REFERENCE_20261121.md`.

## Проверки

На software GLES прошли **162 compile/link пары и 26 pixel checks**: старые
и новые lmap/vertex/local-light формулы, UNORM saturation, fog interpolation,
sky/cloud RGBA, tree lighting/alpha boundary, LOD color/alpha и still/wave grass.
Это реальные GLSL исходники репозитория, EGL/Mesa, синтетические входные текстуры.
Это не доказательство полной сцены на Adreno или декодирования всех игровых DDS.

На SM-X710 / Android 16 / Adreno 740 через `adb install -r` установлены и проверены
поэтапные локальные APK. Загружен настоящий Кордон из копии сохранения
`r1_checkpoint_20261121`. Сохранены gameplay screenshots, ответы камеры/движения,
engine logs, logcat и термоданные в `logs/20261122`.

| Режим | Физическая проверка |
|---|---|
| Старый native | Загрузка, gameplay, пять направлений камеры с профилем |
| Новый scene/native | Несколько поэтапных APK, загрузка, camera/joystick, два направления sweep |
| R2 | Контрольная обычная APK, Кордон, camera/joystick |
| Старый forward | Контрольная обычная APK, загрузка и gameplay |
| Старый multipass | Контрольная обычная APK, загрузка и gameplay |

Это короткие smoke tests, не полная проверка сюжетных переходов, оружия, NPC,
ночной/солнечной погоды, помещений и всех special materials.
Новые пользовательские сохранения не создавались. Лампа и фонарик в корректной
позиции подвала остаются **непроверенными**; старый невалидный teleport не использовался.

Указанные пользователем `openxray_logs[1].zip` и `[2].zip` по путям INetCache
на момент проверки отсутствовали. Их содержание не выдумывалось. Вместо этого
сначала сохранён текущий engine/app log напрямую с планшета (`user-current.*`),
а затем собраны отдельные журналы собственных проверок. Присланные изображения
использовались как визуальные ориентиры; камера, погода и FOV не идентичны.
По исходникам TouchOverlayView рисует только локальные кнопки и joystick;
сплошной затемняющей заливки у него нет. Чёрный loading overlay отдельно
удаляется до игры. Это проверка кода; отдельный hide/show overlay pixel A/B
на устройстве не проводился. UI исходники не изменялись.

## Производительность и диагностика

Диагностическая APK собирается `build.ps1 -GpuProfile`, включается аргументом
`-gles_gpu_profile`. В обычной контрольной APK перерисовочные hooks исключены
при компиляции. GPU queries асинхронные, EXT_disjoint_timer_query, с проверкой
disjoint/интервалов; glFinish не добавлен. Sampling — каждый 32-й gameplay кадр.

Добавлены native world/traversal/LOD/details scopes, реально отправленные
native base/POINT/SPOT и fallback draws, имена linked programs, triangles,
FBO binds и реальные изменения цветовых/depth attachments. Старые CPU/GPU
pass timers и камера сохранены. Отдельный счётчик всех видимых объектов,
полная связь shader program -> blender alias и замер выделения памяти RT
ещё не реализованы. GPU scope на tile GPU нельзя автоматически трактовать
как изолированную стоимость одного материала.

`tools/analyze_r1_profiles.py` вычисляет sampled P50/P95/P99 по положениям камеры
и сохраняет отсутствующие GPU-результаты как отсутствующие. Сырые файлы:

- `profile-scene-fixed.jsonl`: 318 CPU, 193 принятых GPU samples;
- `profile-scene-reverse.jsonl`: 382 CPU, 381 принятый GPU sample, invalid_drops=0;
- `profile-legacy-native.jsonl`: 325 CPU, 205 принятых GPU samples.

В двух направлениях части sweep timestamps не прошли проверку интервалов:
overflow=false, unfinished=0, invalid_intervals=4. Эти GPU пакеты отброшены;
причина неверных timestamps не установлена. CPU-измерения сохранены.

Пример близких направлений в сохранённой позиции (-209.19,-5.94,-145.35),
рендер **1728x1080**, не FHD. Ветер, NPC, небольшие различия yaw/погоды и порядок
тестов ограничивают строгость A/B. Это не долгосрочная оценка FPS.
Таблица снята до финального исправления HQ/LQ `54c719d`; новый полный sweep
после него не проводился. Финальная APK прошла отдельный gameplay smoke test.

| Режим / направление | CPU frame interval P50/P95/P99, ms | Native base draws | Fallback submissions |
|---|---:|---:|---:|
| Старый native, примерно -X | 13.11 / 17.60 / 20.21 | 165 | 201 |
| Новый scene, примерно -X | 10.44 / 13.35 / 16.13 | 285 | 82 |
| Старый native, примерно +X | 20.26 / 25.48 / 30.71 | 1301 | 714 |
| Новый scene, примерно +X | 21.85 / 28.33 / 31.41 | 1492 | 462 |

Снижение deferred submissions подтверждено, **общий прирост FPS не подтверждён**.
В медленном направлении новый режим в этом прогоне даже медленнее прежнего.
Новый native world занимает около 6.30 ms CPU wall, local lighting R2 — 5.35 ms;
всего около 3434 draw calls и 0.89 млн triangles в sampled кадре.
Просто удалить оставшийся R2 без замены зависимостей нельзя. Также необходимо
переработать native submission/light scheduling, сохранив оригинальные формулы
и порядок LDR-смешивания. Стабильные 60 FPS не достигнуты и не заявляются.

## Оставшиеся материалы и архитектура

Runtime инвентарь shader-program families находится в `profile-comparison.json`
и raw `r1_material` records. Это названия программ, а не полный каталог Lua aliases.

| Наблюдавшийся fallback | Текущая цепочка | R1 reference / остаток |
|---|---|---|
| deffer_base_flat / deffer_impl_flat, impl_flat_d | G-buffer, R2 lights/combine | implicit `impl[_dt]`; установить точные material aliases, перенести |
| deffer_base_lmh_* / *_aref_* | G-buffer, alpha, lights/combine | original default_aref/vertex_aref и соответствующие lmap/vertex states |
| deffer_model_* / deffer_base_* | G-buffer, skinning, ROS, R2 shadows/lights | model LQ/HQ, LightProjector/LightShadows/LightPPA ещё не native |
| model_env_lq, model_def_lplanes/base_lplanes | существующий model/forward fallback | model env и OLR special: отдельный перенос |
| lmapE | существующий emissive/forward path | original emissive state/composition ещё не доказаны |
| waterd, water/olr_ryska | существующая water/forward цепочка | waternew/OLR Lua и depth dependencies ещё не native |
| particle, particle_distort, wmark/simple, vert | существующие effects/forward | сверить исходные R1 blend/fog/ordering |

Полный forward pipeline пока не автономен: G-buffer, shadow atlas, light
accumulation, SSAO (при соответствующих настройках) и deferred combine нужны
fallback-материалам. Их RT всё ещё создаются. Выделение ресурсов не равно
исполнению: native exposure/bloom/volumetric/DOF/mblur ранее отключены, но
само наличие буферов и R2-настроек не доказывает выполнение этих эффектов.

Не завершены оригинальные dynamic shadows/LightProjector, native NPC/оружие/руки,
alpha/transparency/emissive/water/special Lua. Не доказаны полное устранение
серого дальнего рельефа, точное совпадение с PC, все девять контрольных сцен.
Нужен сопоставимый save/camera проблемного дальнего плана и подвала. Доступные
оригинальные level/light/shader ресурсы не являются блокером; отсутствующие
оригинальные lookup textures не подменялись выдуманными значениями.

## Сборки, коммиты, продолжение

Ключевые коммиты: `318b1b5` sky/fog; `3d2dbf8` trees/LOD; `006d453` + `93bff23`
details; `8193fe1` profiling; `697dfef` + `6eed06b` исправление profiler crash;
`423eb7a` attachment counters/drop reasons. Наблюдавшийся profiler crash был
нашей ошибкой обращения к очищенному pass.ps: GLES monolithic linker хранит
linked program и очищает stage refs. Исправление проверено повторными сессиями.
`54c719d` исправляет выбор native tree quality при interning R2 элементов.

Полный перечень файлов: `OLR25_R1_SCENE_CHANGED_FILES_20261122.txt`.
Старые контрольные APK и логи сохранены рядом с новыми в `artifacts` / `logs`.

Обычная локальная контрольная сборка (без compiled GPU hooks):
`artifacts/OLR-R1-scene-final-20261122.apk`, source `54c719d`, SHA-256
`0BE170ACC880651840E54A76CA56A4A2706B77E11CB3A8B9F188A184E8C9D0CC`.
Подробности записаны в соседний manifest JSON. Важно: CMake/xrCore banner в
инкрементальной локальной сборке сохранил старую строку `e3dc84f`; эта строка
не идентифицирует новую APK. Использовать SHA-256, manifest и build log.
APK установлена: хеш извлечённого с устройства base.apk совпал. Все четыре
сохранённых файла savedgames (включая quicksave и olr.sav) и user.ltx остались
побитово прежними. Итоговая проверка: `logs/20261122/final-integrity.json`.
В лаунчере оставлены четыре флага нового режима без autoload/profiling.
Приложение закрыто, KEYCODE_SLEEP выполнен, PID игры отсутствует.

[CI 6eed06b](https://github.com/thithertom39/OLR-ANDROID/actions/runs/44971508250)
успешен, включая 162/26 GLES-проверки. Artifact 14734326600 сохранён в
`artifacts/CI-6eed06b`; ZIP digest совпал с GitHub. CI APK имеет пакет `.olr.dev`
и другую подпись; он не заменяет установленную локальную обычную APK.
Также [финальный CI 54c719d](https://github.com/thithertom39/OLR-ANDROID/actions/runs/44979242606)
успешен. Локальная установленная APK и финальный CI собраны из этого source commit.
Artifact 14737601321 сохранён в `artifacts/CI-54c719d`; ZIP SHA-256
`4AE0294B4F3E9A559EEF74A85D7014F4ADA1C30FBE055BE77CE5F41A3C6F84DD`
совпал с GitHub digest. Проверка 162/26 также подтверждена JSON внутри artifact.
CI APK SHA-256: `C0DAC0D3314E41E7B7DC3653DA4B788944B2F1089259057FA99B5E8D5C5C5C27`.

```powershell
adb install -r "R:\project\OLR-R1-parity\artifacts\OLR-R1-scene-final-20261122.apk"
```

Аргументы нового режима (лаунчер добавляет -olr):

```text
-olr_r1_forward -olr_r1_multipass -olr_r1_native -olr_r1_scene
```

Убрать только `-olr_r1_scene` — native от 21.11; убрать также `-olr_r1_native` —
старый multipass; оставить только `-olr_r1_forward` — старый forward;
убрать все четыре — обычный R2. Не включать launcher No shadows/-nodlights:
он убирает источники, включая фонарик. Не менять gamma/ambient для сравнения.

Продолжить с native implicit/aref и models/ROS/projector/shadows, по runtime
инвентарю. До удаления R2 lights/shadows определить, какие fallback получают их
вклад. Отдельно оптимизировать native batching/light-centric submission после
проверки порядка UNORM saturation и alpha/depth. Не возвращать лимит источников.
Старые forward/native и контрольные APK сохранить как A/B.

Тестовые сессии ограничены 80–170 s плюс запуск; после каждой force-stop и
KEYCODE_SLEEP. Температура аккумулятора в завершённых проверках до 24.9 C,
Android thermal status 0. Это температура аккумулятора, не SoC. Экран выключен
во время сборок/анализа, настройки сна/яркости/защиты не менялись.

На R: произошёл подтверждённый No space left on device. NTFS-сжатием только
созданных здесь `.cxx` кэшей восстановлено около 1 GB; затем сжаты также
generated `engine/android/app/build` intermediates. Сборки повторены успешно.
После обычной контрольной сборки свободного места снова мало. До следующего
большого этапа проверить место; не удалять чужие данные/архивы/оригинальные проекты.
