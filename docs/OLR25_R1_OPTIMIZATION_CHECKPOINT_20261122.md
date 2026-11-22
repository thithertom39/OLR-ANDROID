# Native R1: оптимизация и следующие зависимости — 22.11.2026

Рабочая копия: `R:/project/OLR-R1-parity`, ветка
`feature/olr-r1-gles-parity-20261121`, draft
[PR #8](https://github.com/thithertom39/OLR-ANDROID/pull/8).
Последний код: **cb2d0b3cccf67a827eda0051d10449adea7da268**.
Начальная точка кода — `54c719d`, начальный HEAD — `7e51f9d` (только документы).

Сохранён проверенный этап оптимизации, **не законченный независимый R1**.
Просадка в тяжёлом направлении уменьшилась; стабильные 60 FPS не доказаны.
Модели/ROS/projector/shadows и часть специальных материалов ещё требуют R2.

## Что изменено

1. **Native submission** (`5fa7bf8`). Дедупликация родительских источников,
   дистанционное затухание, цвета, нормализация SPOT и его матрица вычисляются
   один раз на forward-проход. Пересечение с конкретной геометрией и portal
   subspace остаются индивидуальными. Native static не вызывает ненужный R2
   `apply_lmaterial`; world transform сравнивается с фактическим состоянием
   backend, учитывая изменение W внутри tree Render. Порядок visual/base/local
   проходов, depth-equal, ONE+ONE и RGBA8 rounding/saturation сохранены.
   Число источников не ограничивается. A/B: `-olr_r1_light_cache_off`.

2. **Implicit BmmD** (`9a4dc22`). Перенесён реально используемый implicit-detail
   материал: UV lightmap берутся из base UV, hemisphere модулируется base alpha,
   ambient применяется полностью, выходная alpha равна 1. Используется
   вторичная detail-текстура из blender и исходный масштаб из texture metadata.
   Самплер implicit lightmap повторяется, как в исходном blender. Отдельно
   проверен фактический OLR `R:/project/unpack/all/shaders/r1/impl_dt.ps`:
   в нём detail умножается напрямую, без distance fade обычного lmap_dt.
   При отсутствии достоверных detail-параметров материал остаётся fallback.
   Исходные сторонние ресурсы в Git не добавлены.

3. **Opaque alpha-reference** (`3e34c9a`). C++ материалы LM_AREF/V_AREF
   получают native base и POINT/SPOT с авторским целочисленным GREATER-порогом.
   HQ/detail и LQ выбираются по реальной дистанции; порог входит в Shader::equal,
   исключая interning материалов с разным alpha reference. Проверены границы
   0/32/200/255 через фактические GLES-программы. Blend/strict ordering
   поверхности и неподтверждённые специальные Lua-материалы сохранены fallback.

4. **R2 local-light receiver culling** (`2818a5f`, `cb2d0b3`). Собираются
   консервативные world-space bounds фактически отправленной G-buffer геометрии,
   включая HUD и динамические объекты. Если диапазон света не пересекает ни
   одного оставшегося receiver, для него не выполняются R2 visibility/shadow/
   accumulation. Исходный Lights.package сохранён для native POINT/SPOT.
   При GI, volumetric light, недостоверных bounds, неперенесённых detail или
   active-item UI сохраняется полная обработка. Для split scene сбор receiver
   и выбор света заканчиваются после PART-1; обычный R2 сохраняет прежний порядок.
   Первый диагностический вариант 2818a5f безопасно не включал отсечение при
   split scene. Действующий вариант — cb2d0b3. A/B: `-olr_r1_receiver_cull_off`.

5. **Диагностика**. `deferred/` программы отделены от прочих `fallback/`;
   JSONL содержит deferred draws/receivers и skipped R2 lights. Кадровый лог
   `olr_r1_deferred_lights` показывает включение и число пропущенных источников.
   `analyze_r1_profiles.py --tail-samples 32` исключает начальное прогревание и
   выводит также thread CPU для каждого этапа. В обычной APK profiling hooks
   скомпилированы выключенными.

6. **CI** (`5bfa693`). После подтверждённого внешнего Maven HTTP 429 добавлены
   ограниченные повторы только этой ошибки загрузки зависимостей. Ошибки
   компиляции/тестов по-прежнему завершают сборку неуспешно.

Sky multiplier, исходные clouds, fog interpolation, vegetation, detail wind,
LOD и исходные attenuation в этом этапе повторно не переписывались.
Gamma/ambient и настройки качества не повышались/не снижались.

## Методика и ограничения измерений

SM-X710 / Android 16 / native Adreno GLES, **1728×1080**, не Full HD.
Каждый запуск загружал один `r1_checkpoint_20261121` в позиции
`(-209.188293,-5.944948,-145.350281)`. Повороты задавались одинаковыми ADB swipe,
положение персонажа при sweep не менялось. Начальное состояние сохранения,
настройки и аргументы одинаковы. Направления максимально близкие, но не
побитово равны: тяжёлый +X в новых A/B около `(0.99,-0.15,0.02)`;
исходная 54 — около `(0.99,-0.15,0.01)`. Погода начинается из одного save,
но часы/ветер/AI и частоты не замораживались. Это ограничивает точность A/B.

Ниже последние **32 sampled CPU frames** каждого положения (stride 32),
GPU matched 31–32. P99 при такой выборке предварителен. Все raw JSONL и
camera vectors сохранены в `logs/20261122/submission`; файл `final-comparison.json`
содержит все пять направлений и этапы. Исходная baseline54 содержит отдельные
углы с недостоверными timestamp intervals: эти GPU samples исключены, не нули.

CPU — thread CPU внутри измеряемого render scope, с работой драйвера,
исключая descheduling; это **не interval кадра**. GPU — EXT timestamp timeline
между началом и концом рендера без swap. Он может включать ожидание подачи
команд CPU и не равен аппаратному GPU busy time. Значения некоторых отдельных
проходов на Adreno выглядят некорректно малыми; по ним не делается атрибуция GPU
стоимости. Perfetto exposes android.gpu.memory, но не GPU render-stage execution
source. KGSL gpubusy/cur_freq читаются; единственный снимок при final smoke
не используется как A/B или распределение GPU busy time.

### CPU/GPU P50 / P95 / P99, мс

| Сборка/режим | Направление | CPU P50/P95/P99 | GPU timeline P50/P95/P99 | Interval P50/P95/P99 |
|---|---|---|---|---|
| 54c719d baseline | −X | 10.36 / 15.40 / 15.77 | 8.53 / 12.69 / 12.74 | 10.67 / 15.22 / 15.84 |
| Финальная, cull OFF | −X | 10.07 / 14.94 / 15.41 | 8.17 / 11.53 / 12.56 | 10.63 / 16.21 / 16.98 |
| Финальная, cull ON | −X | 9.93 / 15.03 / 15.05 | 9.10 / 11.99 / 12.01 | 10.65 / 15.81 / 17.73 |
| 54c719d baseline | +X | 20.24 / 28.54 / 29.06 | 17.57 / 22.04 / 22.54 | 22.86 / 30.50 / 33.31 |
| Финальная, cull OFF | +X | 20.58 / 29.92 / 31.64 | 20.27 / 31.59 / 32.73 | 21.91 / 30.96 / 32.22 |
| Финальная, cull ON | +X | 16.15 / 23.49 / 24.07 | 18.39 / 24.80 / 26.95 | 17.97 / 23.55 / 24.62 |

В чистом A/B одной APK для +X CPU P50 снизился на 21.6%, interval P95 на
23.9%, GPU timeline P95 на 21.5%. В −X устойчивого выигрыша нет. Относительно
полной исходной версии 54 CPU/interval улучшились, **GPU P95 ухудшился**
(22.04 → 24.80): перенос исходных native материалов не равен бесплатному
повышению быстродействия. Не утверждать общий GPU speedup относительно 54.

### Отправки геометрии и цена проходов

| Измерение, среднее sampled кадра | −X cull OFF → ON | +X cull OFF → ON |
|---|---|---|
| Все draw calls | 1756.4 → 1600.9 | 3507.3 → 2505.1 |
| Triangles | 707183 → 665041 | 898854 → 621646 |
| Native POINT draws | 180.8 → 180.7 | 183.8 → 184.3 |
| Native SPOT draws | 22 → 22 | 29 → 29 |
| Все fallback draws | 35.5 → 36.5 | 172.3 → 174.9 |
| Из них G-buffer graph/HUD | 18.8 → 19.7 | 109.4 → 111.1 |
| Пропущено R2 lights | 0 → 3 | 0 → 13 |
| FBO attachment changes | 23 → 23 | 23 → 19 |

Небольшие различия количества объектов соответствуют близким, но не идентичным
направлениям и живому AI. Receiver culling не убирает fallback-объекты и не
ограничивает native свет. В +X R2 local CPU mean: **5.389 → 0.013 мс**.
Native world CPU mean: 7.357 → 7.603 мс; этот этап отсечением R2 не ускоряется.
Первое кэширование submission отдельно давало 6.717 → 6.108 мс native world
CPU относительно 54, но без убедительного улучшения P95 всего кадра.

По этапам в +X fallback: baseline54 453.7 → cache 453.2 → implicit 253.2 →
aref 167.9; в финальных runs 172–175. В +X implicit material составляет около
196 native draws, новые aref убирают ещё около 85 fallback draws. Названия
реальных GLES linked programs сохранены, это не подсчёт файлов/алиасов.
Общий fallback включает также forward/emissive/HUD, поэтому он не равен
числу G-buffer draws. Безусловного полного удаления R2-проходов не сделано.

Проверен marker/dedup путь основной геометрии; новая очередь не добавляет
второй G-buffer draw для native visual. Агрессивная перегруппировка base/light
и batching не внедрены: coplanar depth-equal поверхности и промежуточное
RGBA8 насыщение не позволяют считать произвольную перестановку эквивалентной.
Не получено аппаратного счётчика всех GL state changes; draw/attachment/program
инвентарь и CPU scopes не следует называть таким счётчиком.

## Проверки

- Локальные C++/NDK diagnostic и normal APK успешно собраны.
- Реальные GLES compile/link: **260 пар**, pixel assertions: **114**, все passed
  на WSL Mesa; проверены исходные режимы/debug variants и новые alpha/implicit.
- Финальный CI также выполнил 260/114 и собрал APK:
  [45016232736](https://github.com/thithertom39/OLR-ANDROID/actions/runs/45016232736).
  Artifact 14752247042, `OLR-Android-arm64-18`, сохранён в `artifacts/CI-cb2d0b3`.
  ZIP digest `2C2C2A561911796DD82D5FD58A27E5EBE34E1E0DD56AB98BE79D01B19E59A228`
  сверён. CI .dev APK подписана иначе и не устанавливалась поверх обычной.
- На физическом планшете: baseline после54; cache; implicit; aref; receiver
  culling; A/B off/on в одной diagnostic APK. Кордон загружался из сохранения,
  новые GLSL загрузились без подтверждённых fatal/GL errors.
- В **обычной** финальной APK отдельно проверены native R1, обычный R2 и старый
  forward; в каждом загружен Кордон, проверены swipe камеры и joystick движения.
  Это короткие regression smoke tests, не полная гарантия отсутствия регрессий.
- Сохранены screenshots; в близких видах A/B не выявлено очевидной потери света.
  Это не pixel-identical сравнение с PC: часы/ветер/camera различаются. Точное
  соответствие эталону, подвал/фонарик и весь набор day/night/ATP сцен не доказаны.
- user.ltx и все существующие savedgames до/после побитово совпали; 18 архивов
  на месте, заново не передавались. `final-integrity.json` содержит проверку.
- Установленная normal APK извлечена обратно и совпала по SHA-256. Игра закрыта,
  экран Dozing. Аккумулятор в сессиях до 24.6 °C, Android thermal status 0;
  это не температура SoC. Сессии до170s плюс запуск, паузы на сборки/анализ;
  настройки сна, яркости, thermal/performance не менялись. Проверки с фонариком
  у границ света, движущимися NPC и нестандартными деформируемыми материалами
  ещё нужны: короткий Cordon A/B не доказывает все игровые сцены.

## APK, запуск, оставшаяся работа

Контрольная normal APK: `artifacts/OLR-R1-optimized-control-20261122.apk`,
SHA-256 `C54AF7B7E15A290CED6FEC15AD4933230022CA761FCFABE81837478BB435D297`.
Manifest рядом; исходный код cb2d0b3, profiling скомпилирован OFF. Старый
CMake/xrCore version banner может быть кэшированным; идентифицировать APK
по SHA-256 и manifest. Все A/B APK сохранены, индекс хешей:
`artifacts/20261122/submission-apk-index.json`.

```powershell
adb install -r "R:\project\OLR-R1-parity\artifacts\OLR-R1-optimized-control-20261122.apk"
```

Аргументы normal/native (оставлены в launcher без autoload и diagnostics):

```text
-olr_r1_forward -olr_r1_multipass -olr_r1_native -olr_r1_scene
```

В diagnostic APK добавить `-gles_gpu_profile -gles_scene_compare`; отключать
только receiver culling для A/B флагом `-olr_r1_receiver_cull_off`. Удаление всех
четырёх native flags выбирает прежний R2; один `-olr_r1_forward` — прежний forward.
Не включать NoShadows/-nodlights: это убирает и фонарик.

Следующий перенос — **Model LQ/HQ вместе с ROS/LightProjector/LightShadows**.
Локальный OLR model_def_hq использует m_plmap_xform/clamp и projected lighting;
model_def_lq использует L_dynamic_props (sun/hemi), а не статическую vertex
формулу. Текущий R2 apply_object масштабирует ROS на0.75; нельзя просто назвать
это исходным R1 или заменить HQ на LQ. NPC/руки/оружие/динамические предметы
по-прежнему fallback. Не добавлена непроверенная модель с потерей этого света.

Также остаются blend/специальные Lua, emissive, water, particle/decal/distortion,
unsupported implicit metadata и полноценные исходные dynamic shadows. Поэтому
G-buffer/SSAO (если включён)/deferred combine и нужные local/sun paths сохраняются;
RT allocation не удалены. Теперь можно пропускать ненужные R2 local passes для
отдельных источников, но не весь deferred pipeline. В тяжёлом направлении
основной следующей целью являются native submission и стоимость GPU native
материалов, а не повторный перенос sky/grass/LOD.

Ресурсы R1 доступны в `R:/project/unpack/all`; отсутствующие proprietary файлы
на этом этапе не являются установленным блокером. Оригинальный проект и main
не изменялись. На R после освобождения пользователем около69 GiB свободно;
временно перенесённый generated cache возвращён на R, APK/логи/код сохранены там.
Изменённые файлы перечислены в `OLR25_R1_OPTIMIZATION_CHANGED_FILES_20261122.txt`.
