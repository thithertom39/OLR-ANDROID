# Полный независимый R1: маршрут реализации и доказательств

Цель взята из пользовательского `goal-objective.md` от 22.11.2026.
Она не сужена до оптимизации гибрида, успешной сборки или отдельного материала.
Приоритет: самостоятельная архитектура → полная функциональность → оптимизация.
Работа только в OLR-R1-parity, существующем draft PR8. Начальный HEAD1bc746a,
последний проверенный код cb2d0b3. Исходный проект и пользовательские данные защищены.

Предыдущий этап — progress: сохранены коммиты, APK и реальные tablet A/B.
Текущая глобальная цель остаётся активной; её завершение пока не доказано.

## Карта архитектуры

| Узел | Самостоятельный R1 | Контрольный гибрид / R2 |
|---|---|---|
| Выбор до кадра | отдельный render_olr_r1 | прежний render_deferred |
| Visibility/geometry/resource backend | общие существующие компоненты | те же компоненты |
| Render targets | RGBA8 scene, shared menu/distortion, shared depth | существующие MRT/HDR/shadow/luminance |
| Свет | исходные lmap/vertex/POINT/SPOT + R1 model/projector/shadows | прежний deferred lighting |
| Game PP/display gamma | общие game PP и Android present gamma | прежняя цепочка |
| Неперенесённые материалы | явная блокировка независимого режима | сохранены для безопасного A/B |

Пробный флаг `-olr_r1_independent` дополняет четыре проверенных native flags.
До завершения покрытия это режим разработки, не готовый игровой R1. Он не должен
незаметно возвращаться в R2 или показывать сцену с исчезнувшими объектами.
Обычный четырёхфлаговый гибрид и старые APK сохраняются отдельно как контроль.

В самостоятельном режиме запрещённые deferred entry points имеют runtime guard.
Лог целевого allocator сообщает фактическое наличие G-buffer/accumulator/shadow/
luminance/bloom targets. Нулевая аллокация не заменяет проверку вызовов за кадр.
Заблокированный материалами кадр также не является доказательством готового R1.

## Этапы и необходимые доказательства

1. **Основа и изоляция.** Отдельный frame route и allocator, запрет вызовов
   G-buffer, R2 local/sun accumulation, shadow atlas, SSAO/HDR/bloom/volumetric/
   combine; не дублировать backend. NDK + реальный запуск/логи, проверка R2.
2. **Полный материал-инвентарь.** Сопоставить shaders.xr, shader strings всех
   level и Lua с native/deferred/shared-forward. Runtime дополнить HUD, NPC,
   particles/decals/special. Статический инвентарь не заменяет реальный gameplay.
3. **Динамические объекты.** Перенести Model LQ/HQ, исходный ROS, LightProjector,
   LightShadows/LightPPA, skinning/HUD, weapon/NPC/physics. HQ не заменять LQ;
   R2 ROS с коэффициентом0.75 не выдавать за R1. Проверять source equations,
   atlas/projection/UV bounds/cache lifetime и реальные сцены.
4. **Остальные материалы.** Implicit без metadata, blend, emissive, waternew,
   water variants, particles, wallmarks/decals, distortion, специальные OLR Lua.
   Для shared-forward доказать отсутствие зависимости от R2 targets/lighting.
5. **Совместный кадр/визуальная точность.** Убрать coverage blockers только при
   рабочей реализации. Подвал/torch off-on, пересекающиеся lights, день/ночь,
   NPC, portals, ATP/дальний план: одинаковые camera/time/weather/FOV сPC.
   Отдельно подтвердить тёмные материалы, sky/fog/серый рельеф; не менять gamma
   и ambient для маскировки. Сохранить все текущие source-derived lighting fixes.
6. **Оптимизация самостоятельного R1.** Контролируемые диагностические A/B,
   CPU submission и GPU execution/busy отдельно, draw/state/FBO/overdraw/
   alpha/VS/PS/bandwidth/sync. Сохранять порядок RGBA8/depth-equal/ONE+ONE,
   не снижать качество/плотность/distance/textures и не ограничивать lights.
   Повторяемые R1/R2/предыдущая APK, 1920×1080/60FPS на TabS9; если цель не
   достигнута — измеренные причины. Короткие сессии, thermal/battery ограничения.

## Reference и границы доверия

Оригинальные данные OLR: R:/project/unpack/all (read-only, не публиковать).
Публичный reference OpenXRay/xray закреплён на
`d7b23596a70374d8a7ffda0e98852f93ce985182`; локальные копии находятся только в
ignored artifacts/reference-r1-d7b2359. Он не объявляется точным исходником
модифицированной OLR PC DLL. Фактические OLR Lua/HLSL имеют приоритет при
подтверждённом отличии. Предыдущие ошибки идентификации impl vs lmap не повторять.

## Completion audit

Цель остаётся незавершённой, пока не доказаны ВСЕ требования пользовательского
файла: архитектура и ресурсы, все используемые материалы/свет/эффекты, визуальные
контрольные сцены, стабильность, измеренная производительность, отдельный R2,
сохранённые исходники/APK/CI/SHA/логи/команды и честные ограничения.
Сборка, тестовый manifest, guard, нулевой счётчик в blocked frame, отдельный
green test или частичный checklist не являются достаточным completion proof.
