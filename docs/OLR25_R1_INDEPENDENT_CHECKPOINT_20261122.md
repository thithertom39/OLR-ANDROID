# Независимый R1: архитектурная основа, 22.11.2026

Рабочая копия R:/project/OLR-R1-parity; feature/olr-r1-gles-parity-20261121;
draft PR8: https://github.com/thithertom39/OLR-ANDROID/pull/8.
Полная пользовательская цель остаётся незавершённой. Это контрольная точка
миграции, а не объявление независимого игрового R1 готовым.

## Реализовано и проверено

- `08236f3`: отдельные `render_olr_r1`/`render_deferred`, отдельный allocator
  RGBA8 scene/menu + shared depth; ранний выбор до R2 sun/rain scheduling.
  Нет выделения G-buffer, accumulator, R2 shadow atlas, luminance и bloom.
- Runtime guards стоят на реальных R2 entry points. Число вызовов считается,
  вход в запрещённый этап завершает диагностический запуск с именем этапа.
- Unsupported-material inventory блокирует неполный кадр и возвращает в меню.
  Геометрия не удаляется, R2 fallback не включается за спиной пользователя.
- CTorch spawn сначала падал: `light::set_texture` пытался компилировать R2
  spotlight blender, которого новый allocator намеренно не создаёт. Исправление
  сохраняет оригинальную R1 семантику: fixed SPOT material с authored
  internal_light_att/attclip; per-light R2 accumulation shaders не создаются.
- Обычный R2 и прежний четырёхфлаговый native-гибрид оставлены рабочими.
  Оба загрузили Кордон, camera swipe и движение проверены на SM-X710.
- `1d2ff94`: read-only инвентарь метаданных всех 18 level, 1400 OGF,
  3097 model texture chunks, 1377 particle definitions/1372 sprites и 252
  shader descriptors. Найдено 96 используемых alias, ошибок чтения нет.
  Это перечень ссылок, включая потенциально неиспользуемые в сюжете данные;
  он не доказывает полноту runtime/script-only coverage или визуальную точность.

Архитектурный запуск `independent-torch` на Tab S9: save загружен, сработал
readiness guard. Фактический лог allocator:
`Gbuffer=0 accumulator=0 shadow_atlas=0 luminance=0 bloom=0`.
Первый проверенный кадр: `unsupported=54 native_draws=0 deferred_calls=0`.
Из них 19 submissions моделей, 33 particle submissions, одна emissive queue
и одна sorted queue. Queue counts здесь не являются количеством объектов.
**Игрового изображения независимый режим пока не формирует.**

## Доказательства

Все пути ниже относительно R:/project/OLR-R1-parity:

- `logs/20261122/independent/foundation-torch-build.log`: NDK/APK success.
- `logs/20261122/independent/foundation-gles.json`: 260 compile/link и
  114 pixel checks, passed; Mesa llvmpipe GLES3.2, не Adreno parity.
- `logs/20261122/independent/independent-torch-openxray_u0_a372.log`:
  allocator/readiness/call guard; `independent-torch-80s.png` показывает меню.
- `logs/20261122/independent/independent-r2-control-*`: Cordon + touch input.
- `logs/20261122/independent/native-control-*`: Cordon + touch input в гибриде.
- `logs/20261122/independent/material-inventory.json`: полный локальный перечень
  alias, source references, классов и консервативного статуса реализации.
- User.ltx и savedgames SHA-256 до/после foundation-тестов совпали. Изменились
  только автоматически создаваемые cdb_cache файлы; пользовательские данные
  не восстанавливались из старых копий и не перезаписывались.
- Короткие тесты 80–90 сек, battery 22.3–22.9°C, thermal status0.
  После каждого игра остановлена, экран выключен. Температура аккумулятора
  не объявляется температурой CPU/GPU.

CI для `1d2ff945eeac1fa0ffd6022bbdbb2edcb82bc72f` успешен:
https://github.com/thithertom39/OLR-ANDROID/actions/runs/45033214077.
Локально `artifacts/CI-1d2ff94/`, artifact14759325301.
GitHub digest ZIP совпал с локальным SHA-256:
`c9cd498293339480fcd2a39c88e4fbe4a30c0317e87ca89ce64ed18f651d3279`.
CI .dev APK SHA-256:
`4DDAABBB0CAF4D81C79EF43715387566CCDE5887C997F6A63CF3A0EC690EC8DE`.
CI APK с временным ключом не устанавливалась поверх пользовательского пакета.

Локальная foundation APK с совместимым ключом и compiled profiling hooks:
`artifacts/OLR-R1-independent-foundation-torch-20261122.apk`, SHA-256
`C4F2A6F675C847406B5AAFA94F0177974C14E6C03360FD214FE8A708EA0FE904`.
Обычный profiling runtime в тестах выключен. Первый упавший вариант сохранён
отдельно с crash logs; он не является контрольной игровой сборкой.
Старая проверенная `OLR-R1-optimized-control-20261122.apk` не перезаписана.

## Запуск и ограничения

Экспериментальная проверка readiness:

```text
-olr_r1_forward -olr_r1_multipass -olr_r1_native -olr_r1_scene -olr_r1_independent
```

Без последнего флага — сохранённый гибрид для A/B. Без всех пяти — R2.
Параметры неизменяемы в процессе: для переключения требуется перезапуск.
Пока проверен только MSAA off, не заявлена поддержка независимых multisample
targets. LDR present/game PP написаны, но самостоятельный полный world frame
и его blit/PP не проверены из-за действующего readiness blocker.

Нет новых сравнительных FPS этого режима: заблокированный кадр/меню нельзя
сравнивать с полноценной сценой. Ранее измеренные результаты гибрида относятся
к cb2d0b3 и приведены в OPTIMIZATION_CHECKPOINT, не к независимому R1.

## Дальнейшая работа

1. Model LQ/HQ + skinning, исходные projected lighting и LightProjector cache,
   LightShadows/LightPPA, HUD/NPC/weapon/physics. HQ не заменять LQ.
2. Остальные forward/implicit/blended/emissive/water/particle/special материалы,
   wallmarks, distortion, environmental effects. Убирать readiness blockers
   только после реализации или доказательства общего независимого forward.
3. Проверить полный самостоятельный кадр, game PP, MSAA, R2 regressions,
   весь набор PC/tablet контрольных сцен. Сохранить source-derived lighting.
4. Затем оптимизация самостоятельного R1 и честные FHD CPU/GPU/FPS A/B.

## Продолжение: ROS и стандартные частицы

`4e6b527070fc42e91f5f5a755f1f38017f956db0` переносит original scalar ROS:
порядок 26 направлений, sky/static ray tests, sun/both ray test, исходное
сглаживание, выборку STYPE_LIGHTSOURCE, ray-cache видимости локального света,
временную энергию, линейное distance attenuation и средний RGB luminocity.
R2 cube/polynomial attenuation и build.lights proxies в independent не нужны.
Default `r1_dhemi_scale=1`, bounds0.5..3 взяты из pinned reference, не подобраны
по яркости. Shared sample count/smoothing оставлены с исходной семантикой.
R2 и гибрид продолжают использовать прежний ROS. Static light shadow flag и
консольный `r1_dlights` также согласованы с оригинальным R1.

На TabS9 independent-ros дошёл до ROS с 5 initial samples и исходными
scalar values, затем штатно сработал material guard. Это проверка исполнения
первой выборки, не полная проверка 26 лучей/динамики или освещения NPC.
После этого обычная normal APK (profiling OFF) прошла отдельные Cordon +
camera/movement сессии в R2 и гибриде (`independent-ros-r2-*`,
`independent-ros-hybrid-*`). Максимум battery23.4°C, thermal0.
ROS APK: `artifacts/OLR-R1-independent-ros-20261122.apk`, SHA-256
`0C661A0642D6A24F9932AFABE58E7A4738789B7FDFDCC22C0A57FB55EE6FDB66`.
Успешный CI: https://github.com/thithertom39/OLR-ANDROID/actions/runs/45036985164.
Артефакт в `artifacts/CI-4e6b527`, GitHub ZIP digest проверен.

`278d8b533652c730ff59cbd8728f65bf52dd951b` добавляет original R1 частицы:
SET/BLEND/ADD/MUL/MUL_2X/ALPHA-ADD, исходный depth-write и sampler clamp,
integer GREATER alpha thresholds200/0, vertex color × texture и fog только
для SET. В independent не используется s_position/soft-particle R2 sampling.
Источник: pinned `trunk/xr_3da/xrRender/Blender_Particle.cpp`, R1 branch;
локальные OLR `shaders/r1/particle.vs/.ps`. Изменён **активный** GLES compiler
`Blender_Particle_deferred.cpp`; desktop R1 source не менялся.

GLES проверка расширена: **262 compile/link и 124 pixel checks, passed**.
Новые проверки включают все blend equations, alpha boundary и fog enable.
На Adreno компиляция/загрузка материалов успешна: в одном captured frame
**39 submissions классифицированы independent-forward**, а оставшийся blocker
содержит 19 model submissions + emissive queue + один sorted item.
Старый `particle|particle|null` исчез из missing-material списка.
`independent-particles-active`: unsupported21, native_draws0, deferred_calls0,
allocator все R2 groups0. **Это покрытие материалов, не экономия 39 draw calls
и не измеренное ускорение:** world frame по-прежнему блокируется до переноса
моделей. Число живых частиц меняется между запусками; старый total считал sorted
queue одной записью, новый инвентарь считает её items отдельно.

Normal APK активного particle compiler:
`artifacts/OLR-R1-independent-particles-active-20261122.apk`, SHA-256
`C7B2C6F0FB2D67E182418F82785BE77EE1DEFD5B5011E436093368B53D3DB863`.
Она проверена на планшете в readiness-сессии; последующие изменения того же
коммита касаются только labels compiled profiling ON и статического inventory.
Финальная APK и installed hash фиксируются отдельным manifest после регрессий.

Runtime/profile classification для разрешённых independent draws относится к
native base/forward, а не автоматически к G-buffer по номеру priority.
Readiness whitelist допускает только явно перенесённые программы; generic
R2 particle, Lua xadd/xdistort и непроверенные эффекты остаются blockers.

Следующая конкретная задача: исходный HQ LightProjector + model shader/queue
вместе с LQ/props binding и original shadows. Reference atlas512×512,
100 slots51×51, radius-dependent clipD, cache bbox/TTL, top-down sector-box
surface rendering. Нельзя выдавать очищенный approximate-color atlas за
готовый projected lighting и нельзя принудительно назначать всем моделям LQ.

## Финальная контрольная сборка этого этапа

Код `278d8b533652c730ff59cbd8728f65bf52dd951b`.
Normal APK: `artifacts/OLR-R1-independent-checkpoint-20261122.apk`, profiling OFF.
SHA-256 `C17546CCFAC9A1EEB408FC211E921300F2BD9098AC6E39E6191B9D04BE12B07C`.
Она установлена на основной пакет через adb install -r. Cordon + touch look/move
проверены в обычном R2 и прежнем native-гибриде (`independent-final-r2-*`,
`independent-final-hybrid-*`). Скриншоты показывают игровую сцену, не Activity
или одно главное меню. Известные ограничения темноты/визуальной точности и
направленных просадок этим этапом не объявлены исправленными.

Успешный CI: https://github.com/thithertom39/OLR-ANDROID/actions/runs/45040235796.
Артефакт14761367597, локально `artifacts/CI-278d8b5`.
ZIP SHA-256, совпавший с GitHub digest:
`d65b942f2571557b7ae78a64308bab29cfd435814fd1fa6f9f56c83d3e97fb27`.
CI .dev APK SHA-256:
`3E17907EACEB9694CADEC9D4B9DCA3663BE1558BD07785699A19ED9A4E923691`.
CI также прошёл262/124 GLES checks. CI .dev APK не устанавливалась.
Diagnostic compiled-profiling APK сохранена отдельно; её сборка проверяет
также новую ветку accounting native-forward, которую normal build исключает.

Команда установки normal APK:

```powershell
& 'C:\Users\User\Documents\Codex\2026-10-11\gjvyb\work\android-sdk\platform-tools\adb.exe' install -r 'R:\project\OLR-R1-parity\artifacts\OLR-R1-independent-checkpoint-20261122.apk'
```

Для безопасного игрового контроля оставить четыре native flags, без
`-olr_r1_independent`. Последний включать только для readiness-проверки;
на Кордоне ожидается возврат в меню с перечислением blockers. После каждого
короткого теста закрыть игру и выключить экран. Nine-scene PC/tablet parity,
MSAA, game PP и полный independent world frame ещё не проверены. Возврат в
меню и нулевой call counter не заменяют эти проверки.

Изменённые файлы: `OLR25_R1_INDEPENDENT_CHANGED_FILES_20261122.txt`.
Коммиты архитектуры/инвентаря/ROS/частиц разделены:08236f3,1d2ff94,4e6b527,278d8b5.
Сторонние оригинальные ресурсы не добавлены; original проект/main не менялись.

Публичный reference закреплён на OpenXRay/xray d7b2359. Он не объявляется
идентичным модифицированному OLR PC DLL; локальные OLR shaders/resources
остаются read-only, не включены в коммиты.
