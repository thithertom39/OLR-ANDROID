# Skadovsk: перенос исправлений на ragdoll5, 31 октября 2026

Статус: ragdoll5-skadovsk1 собрана, проверена и установлена на Tab S9.
Замеры и визуальная проверка у Скадовска пока не выполнены. Настройки графики не менялись.

APK: C:\Users\User\Documents\Codex\2026-10-11\gjvyb\outputs\openxray-soc-ragdoll5-skadovsk1-arm64.apk
SHA256: 6b269d50f5c10d4514803822eb4f91a49672d95e5912fc16607940b346694d29
Версия: 1.6.02-ragdoll5-skadovsk1, 33709736 байт.

## Проверка архива и перенос

Архив: C:/Users/User/Downloads/openxray_skadovsk_fix.zip.
Содержимое, SHA и исходные README/тесты сохранены в ../skadovsk-20261031/archive/.
Старые полные исходники из архива не накладывались на рабочие файлы.
Выделены шесть узких кандидатов по текущим preimages:

- src/xrCore/LocatorAPI_defs.cpp::FS_Path::_update: убрать предварительную
  проверку существования для уже lowercase относительных имён; для смешанного
  регистра сохранить исходное написание при наличии в VFS/на диске и прежний
  lowercase fallback, проверяя индекс раньше физического файла. Нет отрицательного
  кеша; конечные FS.exist/read/write и форматы файлов не меняются.
- src/Layers/xrRender/light_vis.cpp::light::vis_prepare: при выключенном HWOCC
  свет видим, pending=false, query_order=0; без объёма для проверки видимости.
  GI подготовка сохранена; обычные accum_point/accum_spot отдельно вычисляют xform.
- light_smapvis.cpp::begin/end/flushoccq: пропустить только query-only
  обучение/дополнительный render_graph и устаревшую invisible-маркировку.
  Нормальный shadow pass, его счётчики и сброс feedback остаются.
- light_smapvis.h: инициализировать frame_sleep/test_count/test_current,
  testQ_V/testQ_id/testQ_frame.
- r__occlusion.cpp: не выделять query pool при enabled=false; disabled begin
  присваивает ID=iInvalidHandle. r__occlusion.h: const accessor is_enabled.

Все три предложения применимы; в исходной базе они ещё отсутствовали.
GL default policy остаётся прежней: HWOCC выключен без -occq для Native GLES
и ANGLE. Флаг -occq не включался. Существующий риск вложенных запросов
в его экспериментальном включённом режиме этим переносом не устраняется.
Путь нормального освещения, теней, эффекты, AA и разрешение не отключаются.
GPU/изображение пока требуют проверки на устройстве.

## Сохранность более новых исправлений

Свежая сверка11728 исходных файлов выявила рассинхронизацию рабочего R и
проверенного снимка ragdoll5. Четыре защитных изменения из более новой ragdoll5
перенесены вперёд отдельными проверенными hunks, с сохранением preimages:
XMLDocument::ClearInternal обнуляет root pointers; xml_str_id_loader освобождает
заимствованные имена при очистке; SoundRender_CoreA задаёт нулевую скорость
listener; device.cpp сохраняет проверенный watchdog register snapshot.
Это сохранение существующего поведения ragdoll5, а не новые Skadovsk-оптимизации.
Текущее удаление неиспользуемого jitter-вычисления в
r3_rendertarget_accum_point.cpp сохранено отдельно как baseline carry-forward.
Исправление AMotor ragdoll5 в joint.cpp остаётся побайтно неизменным.

Доказательства: ../skadovsk-20261031/current-source-baseline.json,
current-vs-ragdoll5.patch, carryforward-manifest.json, carryforward-ragdoll5.patch.
Никаких reset/clean/потери пользовательского кода. Все12 предыдущих APK сохранены.

## Доступные проверки

FS: сравнение реальной текущей функции и кандидата,96 комбинаций путей,
alias buffers, новый unindexed mixed-case loose file,3030 звуковых путей;
POSIX/Windows ветви в optimized и ASan/UBSan. Лишние физические probes
при сборке lowercase-путей в host модели3030→0. Это не замер всех I/O/FPS.
Проверка real stat на временных файлах и полный FS NDK ARM64 TU: PASS.

Occlusion: настоящие перенесённые методы с counted GL/scene doubles;
200 нормальных shadow passes и0 query-only проходов при disabled; GI и свет
видим даже при будущем frame2test/stale pending. Enabled path сохранён.
100 create/destroy/reset циклов, частичные/полные ошибки выделения и рост пустого
пула; все выделенные handles освобождены однократно. Optimized, ASan/UBSan/leaks,
ARM64 syntax для extracted-method fixtures: PASS. Это не настоящий GPU.

Сырые логи/fixtures/results: ../skadovsk-20261031/fs-tests/ и occlusion-tests/.
Новые изменения не затрагивают AMotor: повтор тех же ragdoll host-тестов без
новой гипотезы не выполнялся; SHA joint.cpp зафиксирован в build manifest.

## Сборка, исходники и установка

Release assembleRelease: PASS,9м30с,47 задач. Подпись v2 прежняя,29 ARM64
библиотек совместимы с16KiB pages.2198 игровых assets, ресурсы лаунчера,
ANGLE и C++ runtime побайтно сохранены; допустимое отличие generated ART010
profile — только исходный DEX checksum. Все12 предыдущих APK неизменны.
Шесть проверенных candidate hunks применены к актуальному проекту; повторная
проверка11728 входов active source/frozen stage и targeted git diff --check: PASS.

ADB install -r: Success. Установленная версия/полный APK SHA совпадают с
проверенной сборкой. Обновлено поверх ragdoll4diag без uninstall/data clear.
До/после идентичны все18 защищённых файлов конфигов/сейвов обеих игр;
резервные конфиги и SHA списки — в C:\Users\User\Documents\Codex\2026-10-11\gjvyb\work\skadovsk-20261031\device-install-20261031-015142-491378.
Игровые запуски/измерения/визуальный smoke в этой сессии не выполнялись.
Git commit/push не выполнялись.

## Оставшаяся аппаратная проверка

На устройстве ещё нужны: одинаковый сейв/камера/разрешение/AA/effects/backend,
холодный первый взгляд к Скадовску,30с неподвижно, повторные повороты и прогретый
проход. Сопоставить frametime/FPS, нагрев и подгрузку; проверить голоса NPC,
фонарик и местные динамические тени, SoC/CoP save/load и ragdoll5 в игре.
Никаких цифр прироста или заявления об устранении фризов до этих замеров.


## Аппаратная обратная связь после установки

Пользователь повторил тест плоти в ТЧ: «всё отлично». Read-only логи
01:55–01:58 новой версии без crash/ANR/watchdog stall. Расширенный
-ragdoll_audit в этом запуске выключен; численные physics metrics отсутствуют.
Старые предупреждения ресурсов/скриптов зафиксированы отдельно; это не новая
авария. Новый запуск ЗП и сравнение у Скадовска на момент снимка ещё отсутствуют.
Данные и review: ../skadovsk-20261031/device-check-20261031-015755/.
