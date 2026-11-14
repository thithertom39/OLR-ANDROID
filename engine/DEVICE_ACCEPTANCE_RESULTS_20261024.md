# Аппаратная проверка OpenXRay: 24–25 октября 2026

## Результат и границы проверки

Проверены исходная thermala1, stabilityaudit1 и отдельный новый кандидат stabilityaudit2 на Galaxy Tab S9 / SM-X710, Android 16, serial R52X40ES4LE, package org.openxray.dev, UID 10361. Root был единственным оператором ADB. Все исходные APK сохранены. Изменения кандидатов не объединены в рабочую сборку.

**Полная приёмка стабильности НЕ пройдена.** Выход и изменение настройки после выхода прошли на stabilityaudit1 с Native GLES и ANGLE и на stabilityaudit2 с Native GLES. На stabilityaudit2 повторное открытие главного меню прошло, но повторная загрузка того же сейва завершилась новым native crash. Серия из десяти полных циклов на каждый backend не выполнена: проблемные блоки остановлены после воспроизведения ошибок.

Доказательства и отдельные патчи находятся в:

`C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/device-acceptance-20261024-204608`

Далее этот каталог обозначен D. W — `C:/Users/User/Documents/Codex/2026-10-11/gjvyb`, R — W/work/openxray-build. Ничего из временных каталогов/сырого logcat не публиковать автоматически: общий logcat содержит сообщения других приложений.

## Сначала выполненное восстановление устройства

Незавершённое восстановление после thermal-сессии закрыто до установки новой APK. Первая успешная запись:

`W/work/thermal-power-20261023/restore-today-204643-128301/result.json`

Восстановление повторялось между блоками. Финальная запись:

`W/work/thermal-power-20261023/restore-today-001126-433460/result.json`

Она подтверждает 28 исходных UI-состояний после перезапуска процесса, точный исходный user.ltx и все 12 исходных save-файлов. Финальная свежая независимая проверка 25 октября 00:38 MSK:

`D/final-device-verification/result.json`

Подтверждены: user.ltx 11673 B, SHA256 `b129ad651af57f7191734fe91dc98a80d9a1e85931e978a826bccbf94dd568b8`, 12 save hashes/sizes, установленный APK stabilityaudit2, отсутствующий PID игры, экран Dozing и thermal status 0. Настройки возвращены к Native GLES, 1728×1080, лимиты 501/501, обычное игровое время; временный FHD/.001-time/cap60 профиль не оставлен. Private SharedPreferences XML побайтово не копировался; проверены 28 UI-состояний предусмотренным helper. Сохранения не записывались и не удалялись.

## Найденные ошибки

### DEV-001: native abort примерно через 12 секунд после выхода — воспроизведён

На thermala1 PID23533 штатный engine quit завершил SDL main 24 октября 20:49:57.475; игровая Activity прошла onDestroy 20:49:58.022. После возврата в существующий лаунчер изменён check_nointro. В 20:50:09.553 оставшийся OpenXRayFT watchdog сообщил STALL; тот же TID23747 затем вызвал abort Bionic `invalid pthread_t ... passed to pthread_kill`. ApplicationExitInfo: SIGNALED/status6 в 20:50:09.610.

Это настоящий native abort, а не Java exception, ANR или вывод по одному crash marker. Полного fatal backtrace для этого события нет; связь подтверждена последовательностью логов, идентичным потоком и исходниками старого watchdog. Дефект существовал до stability-аудита.

Подробнее: `D/BASELINE_CRASH_FINDING.md`, `D/thermala1-crash-buffer.txt`, `D/thermala1-exit-info.txt`, `D/logcat-live.txt`.

В frozen stabilityaudit1 уже есть FT_SHUTDOWN первым действием CApplication::~CApplication и stop/wake/join watchdog в device.cpp. В проверенных случаях Native/ANGLE после quit выдержаны дополнительные 18 секунд, PID сохранился и переключение check_nointro туда/обратно прошло. Это ограниченное аппаратное подтверждение конкретного исправления; не универсальная приёмка всех lifecycle-переходов.

### DEV-002: второй native main использует прежние имена XML — воспроизведён

stabilityaudit1 PID25299 после успешного первого выхода и настройки повторно запущен без force-stop. В 20:55:53.319 получен SIGTRAP: XMLDocument::Load пытался открыть `gameplay\down:b12.xml`.

Совпавший Build ID libxrGame `b7b8372a8bfc56467017e84f7e9c1e2c83852c88` и llvm-addr2line указывают на CXML_IdToIndex<CEncyclopediaArticle>::InitInternal, xml_str_id_loader.h:140, вызванный init_game_globals в xrgame_dll_detach.cpp:47.

DeleteIdToIndexData удалял XML-индекс, но оставлял static LPCSTR file_str, заимствованный из pSettings. После уничтожения настроек/Core следующий InitXmlIdToIndex пропускал присваивание ненулевого старого file_str. Все пять владельцев шаблона используют такой guard. Четыре относящихся к этому исходника совпадают с pre-D1 baseline; это не доказанная регрессия D1.

Подробнее: `D/REPEAT_START_CRASH_FINDING.md`, `D/stability-repeat-crash-buffer.txt`, `D/stability-repeat-exit-info.txt`.

### DEV-003: повторная загрузка сейва после нового native main — воспроизведена, открыта

В stabilityaudit2 PID6120 первый сейв загрузился, quit + 18 секунд и настройка прошли. Второй native main в том же процессе успешно создал и показал главное меню. Команда `load cg22benchp4` затем привела к SIGSEGV 25 октября 00:04:04.019, TID8075; ApplicationExitInfo подтверждает status11. Новый PID8861 относится к восстановленному лаунчеру, а не к успешному игровому сеансу.

Native stack: XMLDocument::NavigateToNodeWithAttribute → CGameTask::Load → CGameTask::load → SGameTaskKey::load → CALifeRegistryContainer::load → CALifeStorageManager::load.

Есть сильное исходное объяснение: CGameTaskManager хранит inline static CUIXml m_gameTaskXml; его destructor вызывает ClearInternal. XMLDocument::ClearInternal очищает DOM, но не m_root/m_pLocalRoot. Следующий Set обновляет m_root, но прежний ненулевой m_pLocalRoot может остаться; NavigateToNodeWithAttribute предпочитает его новому корню. Прямое аппаратное подтверждение исправления этого механизма пока отсутствует.

Доказательства: `D/stability2-repeat-load-all-logcat.txt`, `D/stability2-repeat-load-exit-info.txt`, `D/sa2-native-load2-000346-879312/` (включая engine.log, screenshot и app logs). Не повторять загрузку в аварийном процессе и не считать этот блок пройденным.

## Новая независимая stabilityaudit2

APK: `W/outputs/openxray-soc-stabilityaudit2-arm64.apk`, 33698908 B.

SHA256: `d3acce465bcadb0d44cf7d83e36576dceb95eca71745a970173fb31f1e6f5c00`.

Источник: ровно 11726 frozen inputs stabilityaudit1. Единственная правка поведения — обнуление file_str и tag_name после удаления XML-индекса в src/xrServerEntities/xml_str_id_loader.h. Вторая разница входов — только путь отдельных dependency build directories в CMake. Thermal A, TaskQueue, декомпрессия, EGL и менеджер модов не добавлены.

Рабочий R и frozen W/work/stab-build не изменялись ради этого патча. Исходный патч/копия заголовка/manifest находятся в `D/repeat-start-fix/`; header ещё не интегрирован в R. Отдельный новый stage: W/work/stab2-build; dependency builds: W/work/stability2-deps. Не переиспользовать старые wrapper для новой версии.

Host regression: actual template header и пять извлечённых initializer bodies, 100 поколений настроек, независимое пересоздание владельцев, 33 случая пустой optional encyclopedia, ASan/UBSan/leak checks; первый сеанс эквивалентен, исходная ошибка обнаруживается, удаление обязательного сброса определяется тестом. Окружающие XML/FS/settings — модели, не целый движок. Actual ARM64 API24 NDK27 xrgame_dll_detach.cpp compile с external overlay PASS. Первая некорректная standalone compile без исходного forced textual include сохранена отдельно; при исправлении harness production flags не ослаблялись.

Полная сборка PASS за 9m20s, 47 tasks, Java и lintVitalRelease. APK verifier PASS: прежняя подпись/v2, package/version, 29 ARM64 библиотек, 16 KiB alignment, прежние ANGLE и C++ runtime, GL dispatch и старые mapfix/task guards, отсутствие embedded token и тяжёлой GPU diagnostics. Это не аппаратная приёмка.

Pipeline и frozen доказательства: `D/pipeline/README.md`, `stabilityaudit2-inputs.json`, `stabilityaudit2-apk-report.json`, build log. Reviewed plan SHA256 `2cd34d6e0fb0fec260199445ba97fc663c2aa918ead816a578bced669a1ab099`.

Аппаратный результат: первый Native load/exit/settings PASS, повторное меню PASS, повторный load FAIL DEV-003. ANGLE в stabilityaudit2 не проверен — остановлено после нового падения Native. Не переносить результаты ANGLE stabilityaudit1 на новую APK.

## Подготовленная, но не собранная следующая правка

`D/xml-document-reuse-fix/` содержит отдельную копию XMLDocument.cpp и XML-REUSE-002-clear-document-cursors.patch. Он только обнуляет m_root и m_pLocalRoot после m_Doc.Clear().

Host test использует реальные извлечённые тела ClearInternal и модель DOM/узлов: исходник нарушает проверяемый post-clear контракт; кандидат проходит 1000 clear/reuse циклов с независимым вторым документом, ASan/UBSan. Это не полный TinyXML/engine test.

Candidate SHA XMLDocument.cpp `6a2642b29ddd7da1d200ae96b13f67fe6942ea710958b7d0295ba348586a67da`, preimage `40e15e161713e23117caa484371e66d2ea800e913ca33ccfff23c33ffe83f44d`.

Actual NDK compile, полноценный XML reload regression, full link/APK, независимая аппаратная проверка и интеграция в R НЕ выполнены. stabilityaudit3 не создана. Следующий агент должен сначала проверить этот ограниченный кандидат, а не считать DEV-003 исправленной.

## Thermala1: короткие Full HD наблюдения

Подробности и 356 проверенных hashes: `D/THERMALA1_SHORT_OBSERVATIONS.md` и JSON.

Все четыре окна — реально загруженный cg22benchp4, мост на Кордоне, 1920×1080 actual FBO, cap60, исходный AA off, без открытой консоли. Камера визуально сопоставима; изображения не объявлены попиксельно идентичными. Это время предъявления кадров SurfaceFlinger, не GPU timer и не CPU frame time.

| Backend | Два окна по 20 секунд | Median interval | P95/P99 | 1% low |
|---|---|---|---|---|
| Native GLES | 45.49 / 46.22 FPS | около 25 ms | около 33.33 ms | около 30 FPS |
| ANGLE Vulkan | 43.87 / 42.21 FPS | около 25 ms | около 33.33 ms | около 30 FPS |

Интервалов >100 ms не было. Thermal status в измеренных окнах 0. Различаются периоды screen-off pause/прогрев и исходные thermal readings; строгой A/B пары backend или прошлой сборки нет. Снижение мощности, температуры, энергии на кадр и FPS gain A-правок НЕ доказаны; не использовать вчерашние control FPS как сегодняшнее сравнение. Пространственный звук не проверен прослушиванием. Thermala1 нельзя считать прошедшей общую stability acceptance из-за DEV-001.

## Ошибки harness, не смешивать с ошибками игры

- Первая попытка load stabilityaudit2 при Dozing не подтвердила ввод. После wake и визуального осмотра меню команда повторена успешно. Helper теперь будит экран перед действием, не меняя глобальных настроек.
- Повторный start первоначально ожидал process-static одноразовую строку first menu render. Она не печатается второй раз. Меню действительно работало; timeout сохранён и отдельно разобран в D/sa2-native-repeat1-harness-review.json. Следующий подтверждённый SIGSEGV при load — настоящий другой сбой, не этот timeout.
- Для quit helper не отправляет ESC после команды: CCC_Quit сам скрывает консоль, завершает движок и возвращает существующий Launcher. Это штатный engine route, но не полное покрытие мышью/касанием кнопки выхода и диалога подтверждения. force-stop не выдавался за проверяемый quit.

## Продолжение

1. Сохранить три старых APK, stabilityaudit2, оба frozen stages, новые доказательства и пользовательские файлы. Не запускать старые finalizer, ожидающие только D2/D3 delta.
2. Прочитать DEV-003 evidence и внешний XML-REUSE-002. Проверить actual XML Clear/Set/reload с local roots, NDK TU и собрать новый отдельный кандидат из frozen stabilityaudit2 через новый reviewed pipeline. Старый stage не редактировать.
3. Сначала Native same-PID menu→save→quit→settings→menu→same save. Только после успешного повтора расширить до серии и ANGLE. Проверять PID, свежие логи, видимый мир и закрытую консоль. Все обычные console commands закрывать ESC; load и quit требуют отдельной визуальной логики.
4. После устранения DEV-003 — повторные циклы, HOME/Recents/lock/resume, остальные D1 fixes и только затем объединение принятых изменений. Настоящая потеря EGL context, TaskQueue и decompression остаются отдельными кандидатами.
5. Не объявлять thermala1 энергоэффективнее без сопоставимых A/B измерений. Пока менеджер модов и серьёзное распараллеливание не интегрируются.
6. На конец этой сессии thermal restore уже завершён. Не считать его по-прежнему pending по историческим D1–D3 документам; при новом подключении проверить свежие данные и не перезаписывать более новые пользовательские настройки/сейвы старым baseline автоматически.

Git reset/clean/checkout с потерей данных, commits, push и объединённый релиз не выполнялись. ПК не выключался: старый запрос shutdown относился к предыдущей завершённой работе, нового запроса в этой аппаратной сессии не было.
