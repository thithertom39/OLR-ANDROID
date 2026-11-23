# R1 заморожен по запросу пользователя — 23.11.2026

Не возобновлять R1 автоматически. Текущая задача переключена на отдельный R2.
Независимый R1 **НЕ завершён**. После этой контрольной точки код R1 не изменять.

Ветка: `feature/olr-r1-gles-parity-20261121`, draft PR
https://github.com/thithertom39/OLR-ANDROID/pull/8 .
Последний renderer commit: `597fdc46e6c2cf72c467dae2185fb54f35b33c50`.
Все исходники опубликованы в этой ветке; основной проект и main не изменялись.

Сборки/тесты локально завершены, активных процессов сборки/WSL/игры не найдено.
Последняя CI 45110354050 SUCCESS:
https://github.com/thithertom39/OLR-ANDROID/actions/runs/45110354050 .
Контрольная CI 1d13b9f (321 shader pairs / 157 pixel fixtures PASS) и проверенный
артефакт сохранены: `artifacts/CI-1d13b9f/verified.json`.

## Состояние компонентов и пределы проверки

- Model HQ/LQ, original ROS, LightProjector реализованы; изолированные
  планшетные проверки: 3 HQ + 15 LQ, 50 projector surface draws.
- LightPPA model POINT/SPOT подключён через sector/portal/frustum selection.
  Контролируемый GPU fixture: SPOT 38625 / POINT 38626 осветлённых пикселей.
  Реальные источники в данном сохранении видимого вклада в модели не дали.
- LightShadows: оригинальный caster atlas, blur, collision receivers, PLC,
  кэш, projected multiply. Fixture: 2 слота/2 caster draws, 76 receiver
  triangles/1 draw, 37212 изменённых пикселей на отдельном empty-depth target.
  Это не проверка полного игрового кадра или оригинальной подвальной лампы.
- 597fdc4 добавляет shadow-only вызов Render_First без R2 shadow phase и
  без color queues. **Физическая проверка дала added_parts=0**: тень тела
  игрока не доказана и считается незавершённой. Не объявлять её исправленной.
- Полный independent кадр остаётся guard-blocked: unsupported=21,
  deferred_calls=0. HUD, порядок проходов, оставшиеся материалы/эффекты,
  полная ПК-похожесть, полноэкранная игра и производительность не завершены.
- R2 и прежний гибрид прошли 90-секундные look/move checks на 1d13b9f;
  после локального 597fdc4 R1-only изменения эти игровые проверки не повторялись.

## Сохранённые APK и данные

Последняя обычная APK, установленная на планшете:
`artifacts/OLR-R1-actor-shadow-597fdc4-20261123.apk`, SHA256
`F1AA924174C445176316A11E7FF3A21A63B9BEBB13A2B2968F4B53EF0F1BFAD8`.

Предыдущая проверенная R2/hybrid control APK:
`artifacts/OLR-R1-shadow-receivers-1d13b9f-20261123.apk`, SHA256
`EA6C1EB2A743C72939344CADB09C77A36131895D2C3179883138FAAEC0E7D757`.

Все старые APK, JSON, RGBA/PNG, GLES/NDK/Gradle/logcat и бэкапы сохранены.
Логи: `logs/20261123/lightppa/`, `logs/20261123/shadows/`.
`tests/__pycache__/` — единственный незакоммиченный генерируемый каталог;
оставлен локально, не добавлять в Git. APK/ресурсы/сохранения/большие логи
не входят в коммиты. Игровые архивы не копировались повторно.

## Точная инструкция возобновления, только по отдельной команде

1. Проверить HEAD/status/процессы; прочитать этот файл, затем
   `docs/OLR25_R1_SHADOWS_20261123.md`, `docs/OLR25_R1_LIGHTPPA_20261123.md`
   и `docs/OLR25_R1_MODEL_LOCAL_RGBA_20261123.md`.
2. Начать с added_parts=0: проверить game-side eligibility Render_First,
   marker/culling и material metadata первого лица. Не менять глобальную
   renderer generation без оценки влияния и не подделывать наличие тени.
3. Затем HUD и оригинальный порядок base/SPOT/POINT/shadows, независимый
   non-model local RGBA/final-alpha, оставшиеся model-environment/emissive/
   sorted/water/Lua. Сохранить guard до функциональной полноты.
4. Сборка — прежний jobs=2 native prebuild и Gradle; команды/SDK/подпись
   сохранены в предыдущих документах и логах. CI APK с другой подписью не
   ставить поверх обычной. Все результаты оставлять на R:.
5. После каждой короткой проверки force-stop игры и screen off. Никаких
   многолетних стресс-тестов/изменений thermal/system settings/перезаписи saves.

R2 разрабатывается отдельно от стабильной R2 базы. Эти незавершённые R1
компоненты туда не переносить.
