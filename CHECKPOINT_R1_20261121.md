# OLR R1 GLES — контрольная точка 21.11.2026

Полный оригинальный R1 ещё **не завершён**. Реализован и проверен следующий
этап переноса обычных статических материалов и локального освещения. Нельзя
выдавать его за полную совместимость с ПК-версией.

## Рабочая копия и ветка

- Новая независимая рабочая копия: `R:\project\OLR-R1-parity`.
- Ветка: `feature/olr-r1-gles-parity-20261121`, дочерняя от `1137caa` PR #7.
- Draft PR: https://github.com/thithertom39/OLR-ANDROID/pull/8, base — ветка PR #7.
- `main`, исходный OpenXRay Android и незакоммиченные изменения интерфейса
  в `R:\project\OLR-Android` не изменены. Fetch обновил только Git metadata.
- Игровые ресурсы читались из `R:\project\unpack\all`; оригинальные DDS,
  уровни, Lua и архивы не добавлялись в публичный репозиторий.

## Реализация

1. Исправлена идентификация `default`: данные `shaders.xr`, строки материалов
   Кордона и исходный BlenderDefault выбирают **lmap/lmap_dt**, не impl.
   Используются `_1.rgb`, `_1.a` для солнца и среднее `_2.rgb` для hemisphere;
   ambient делится на 1.5. Старое утверждение в SOURCE_COMPARISON помечено
   историческим и исправлено.
2. Добавлены native vertex-lit материалы: vertex RGB, hemisphere через N.a,
   солнце через vertex color.a, ambient и исходное удвоение diffuse.
3. POINT: два исходных attenuation lookup, UV XZ/XY, `.5/range`, clamp/linear.
4. SPOT: исходные internal projector/clip, D3D-LH матрица, отрицательный Y,
   смещение `.5/32`, XY/W и Z **без** деления на W. Gouraud освещение и fog
   считаются в вершинах, COLOR ограничивается как вход PS1.x.
5. Diffuse alpha и масштаб 2 применяются к локальному свету. Detail влияет
   только на основной проход, как в исходных add_point/add_spot.
6. Статическая геометрия локальных источников отбирается через sector/portal
   subspace и frustum. Используется отдельный CPU-контекст; кэш действует
   только в пределах кадра. Нет лимита числа источников; сохранены dedup
   POINT-граней, исключение bStatic и spatial-sphere fade r1_dlights_clip.
7. Native base и depth-equal ONE+ONE пишут прямо в RGBA8. В этом режиме
   убраны R2 exposure, bloom, volumetric overlay, DOF и motion blur. Модели
   и ещё не перенесённые материалы продолжают пользоваться R2 fallback.
8. Исправлен реальный GLES вход Lua-компилятора: предыдущий PR менял общий
   ResourceManager_Scripting.cpp, который не входит в xrRender_GL. Native
   элементы теперь подключаются в glResourceManager_Scripting.cpp и C++.
9. Диагностика `-olr_r1_debug=0..6` специализируется при компиляции шейдеров.
   Обычный вариант не выполняет дополнительных диагностических выборок.

Проектор оригинального reference R1 фиксирован: `light::set_texture` там
пустой. Произвольные cookies R2 не объявляются функцией оригинального R1.
Матрицы взяты из публичного reference OpenXRay/xray; тождество конкретному
модифицированному DLL OLR 2.5 отдельно не доказано.

## Сборки и коммиты

- `e3dc84f`: lookup shaders, native materials и LDR.
- `9bed63b`: исправление точки подключения GLES compiler после найденной
  ошибки линковки первой сборки.
- `c1a092b`: portal light subspaces.
- `ee5f754`: native framebuffer path без R2 exposure/bloom/depth PP.
- `29a5df2`: GLES тесты и документация; CI проверял этот SHA.
- Последующий коммит: только документация, воспроизводимый ADB smoke-helper
  и исправление имён reference-функций в комментариях; алгоритмы не меняет.

Успешный CI: https://github.com/thithertom39/OLR-ANDROID/actions/runs/44908650295

Artifact: `OLR-Android-arm64-11`, ID `14708897980`.
Локально: `artifacts\CI-29a5df2\OLR-Android-arm64.apk`, пакет
`org.openxray.olr.dev`, arm64-v8a. SHA-256:
`BE8F16B475434C243C8D720D97E1027EF0595B641A0E91F43A304B0E9E02BB4F`.

ZIP: `artifacts\OLR-R1-native-CI-29a5df2.zip`. SHA-256 совпал с GitHub digest:
`58CD99D94E3E66246A598C72E217B41EB9BDF88949B26ACBCEFE09F18C920F7F`.

Проверенная на планшете локальная APK:
`artifacts\OLR-R1-native-portal-20261121.apk`, пакет `org.openxray.olr`.
SHA-256 установленного base.apk совпадает:
`F08F90697A3B78049CF34DEFD13C72E0A3E6D3DF3A16D3517138D8F8D56C3D60`.
Подпись совпадает с исходной установленной APK, поэтому использован install -r.
CI APK имеет другой пакет/ключ; её не надо подменять подписью локальной APK.

## Подтверждённые проверки

- Локальная C++/NDK/release-сборка и отдельная облачная CI-сборка успешны.
- Настоящий GLES 3.2 Mesa/llvmpipe: 42 пары compile/link, семь draw/pixel
  проверок, включая второй lightmap, detail, vertex, Gouraud POINT, насыщение
  ONE+ONE, SPOT XY/W и неделённый Z, discard позади прожектора.
- Физический SM-X710, Android 16, Adreno 740: один и тот же Cordon save
  загружен до игрового кадра в native, старом forward, старом multipass и R2.
- В native проверены касания камеры и джойстика, сохранены кадры до/после.
- В просмотренных журналах нет fatal/shader compilation crash. Это не
  утверждение об отсутствии всех предупреждений: сохранились, например,
  сообщения отсутствующих ui_scroll текстур и старых console syntax entries.
- Сессии короткие, между ними игра закрывалась, экран выключался. Во время
  записанных игровых проверок battery 16.3–17.1 C, Android thermal status 0.
  Это температура аккумулятора, не процессора/GPU. FPS/P95 не измерялись.

Журналы и PNG находятся в `logs`:
`tablet-native-fourth-*`, `tablet-forward-control-*`,
`tablet-multipass-control-*`, `tablet-r2-control-*`,
`tablet-native-input-cellar-*`, `tablet-native-cellar-recheck-*`.
Последние две группы содержат также неудачные диагностические попытки —
не считать все изображения в них успешными проверками подвала/фонарика.
GLES host-результаты: `logs\r1-gles-probe.json`.

## Что не подтверждено и что осталось

- Эталонное изображение с ПК при тех же camera/time/weather не получено.
  Native кадр темнее старого R2/forward. Это следствие иного пути композиции,
  но само по себе не доказывает соответствие ПК.
- Лампа подвала не идентифицирована. Координаты compiled light #13 не дали
  правильного положения камеры: проба оказалась под геометрией. Не менять
  источнику bStatic и не повышать ambient на основании этого кадра.
- Torch probe не доказывает работу оригинального прожектора на устройстве:
  часть кадров перекрыта console/IME, другая получена в невалидной позиции.
  Нужны подходящее сохранение в помещении и ручная/надёжная автоматическая
  проверка on/off. Телепорты и тестовый предмет не записывались в user saves.
- Остальные из девяти сцен (ночь, пересекающиеся источники, NPC около света,
  переходы через порталы, солнечный АТП) ещё требуют проверки.
- Не перенесены native models LQ/HQ с ROS/LightProjector, оригинальные R1
  динамические тени, vegetation/detail/LOD, alpha/transparency/emissive,
  waternew и специальные Lua-материалы. Их fallback остаётся R2.
- После переноса этих материалов можно убрать оставшиеся ненужные R2
  G-buffer/shadow/light вычисления. Сейчас их удаление сломало бы fallback.

Исходные lookup DDS все найдены. Отсутствующих текстур для выполненного этапа
нет. Для полного соответствия полезны исходники **точной** версии OLR 2.5
PC engine, прежде всего её LightPPA, LightProjector, LightShadows, model/PP
пути, и сохранение/камера непосредственно у проблемной лампы. Публичный R1
reference доступен по ссылкам в `docs\OLR25_R1_REFERENCE_20261121.md`;
не следует выдавать его за доказанный исходник конкретного OLR DLL.

## Продолжение и сохранность данных

Фактические assets на планшете: `/storage/emulated/0/Download/STALKER-OLR2.5`.
Проверены 18/18 архивов; они не передавались повторно. Через Browse сохранён
этот действующий путь вместо отсутствующего прежнего каталога.

Исходные `olr.sav`, quicksave и восстановленный user.ltx совпали по SHA-256 с
копиями до теста. Резервы — `artifacts\user-saves-before-native`,
`logs\user-before-native.ltx`, `artifacts\OLR-before-native-installed.apk`.
Создан только отдельный `r1_checkpoint_20261121.sav` для повторных загрузок;
исходные сохранения не перезаписывались. В лаунчере восстановлены прежние
additional arguments `-olr_r1_forward`, автоматическая загрузка теста убрана.
Игра остановлена, экран выключен, тепловой статус 0.

Для проверки нового режима указать в Additional arguments:

```text
-olr_r1_forward -olr_r1_multipass -olr_r1_native
```

Установка проверенной локальной APK:

```powershell
adb install -r R:\project\OLR-R1-parity\artifacts\OLR-R1-native-portal-20261121.apk
```

Сценарии A/B и диагностика подробно описаны в
`docs\OLR25_R1_GLES_STATUS_20261121.md`. Smoke-helper умеет ограничивать сессию
и завершать игру/гасить экран в finally; вызывать с фактическим путём к ADB и
GameRoot. Не запускать его сериями на часы.

Полный перечень изменённых файлов: `artifacts\CHANGED_FILES.txt`.
Патчи отдельных коммитов: `artifacts\patches`.
Следующий приоритет: корректная контрольная сцена подвала/torch, затем
native models + original LightProjector/LightShadows и остальные материалы.
