# Stability followup: результаты и границы приёмки

23 октября 2026, offline-сессия D2. **Подготовлены независимые кандидаты и тесты по всем пяти приоритетам. Производственный код, настройки, зависимости, сохранения и APK в этой сессии не изменялись.** В R обновляется только документация. Подключение ADB, установка и сборка новой APK не выполнялись. Реальный вылет Launcher→Game→Exit→Settings не воспроизведён и не объявлен устранённым.

## Где сохранён результат

- W = C:/Users/User/Documents/Codex/2026-10-11/gjvyb.
- R = W/work/openxray-build — рабочий репозиторий с принятыми ранее D1 source changes.
- D1 = W/work/stability-20261023 — предыдущий аудит и frozen stabilityaudit1 pipeline, сохраняются.
- D2 = W/work/stability-followup-20261023 — текущие внешние кандидаты, raw logs, fixtures, результаты, планы проверки.
- D2/session-state.json и source-delta-final.json — итоговая машинная сверка; D2/evidence-sha256.json — указатель на доказательства. Патчи не являются применёнными изменениями R.

Исходная D2 копия: source-baseline.zip, **11732 файла, 53453056 байт**, SHA256 dde82eec80a6c044bc1052877ff797b6b325242ec1b31382d576d03ddbf550de. Перед началом проверены ранее изменённые19 source files и7 D1 отчётов. Новый snapshot не заменяет Git HEAD и не разрешает reset. Из snapshot исключены ключи, local.properties, build/cache directories и прочие перечисленные в baseline-manifest.json данные; это не резервная копия всей Windows или устройства.

## Закрыто в изолированных кандидатах / открыто в production

| Направление | Доказанный результат D2 | Статус основной сборки и остаток |
|---|---|---|
| TaskQueue, High | Воспроизведены slot reuse loss, false-empty после проигранного CAS, release-full overwrite и reuse незавершённого allocator slot. Подготовлены queue mutex reference, completion-acquire и allocator guard. | R scheduler **не заменён**. Mutex заметно дороже на host; full queue/allocator пока fail-closed, не graceful recovery. Остальные task handle/membership races не закрыты. |
| ENG-005, High | Safe LZO checked API и все3 известных consumers, проверка archive ranges, cleanup и отказ до unload мира при ошибке codec. Legacy ABI и capacity semantics сохранены после review. | R decoder прежний. Нужны отдельная линковка/APK, retail archives и device. Внутренние game chunks, OOM, archive table parser и actual streaming map/remap остаются отдельными ограничениями. |
| EGL loss, High capability gap | Проверены23 файла, составлена карта всех основных owners/dependencies и порядок generation-aware восстановления. Offline policy model с19 тестами. | **Полного recovery нет**. Обычный working surface resume сохранён. Static buffer recipes/program permutations/aliases требуют более широкого изменения владения, чем безопасный небольшой патч. |
| Launcher crash | Расширены source-extracted lifecycle/log tests. Подготовлен отдельный bounded journal и parser для сохранения переходов при завершении процесса. | Причина реального crash неизвестна. Новый journal не подключён; нужен hardware latency/ANR/export review. Старые D1 fixes остаются pending hardware. |
| Mod foundation | Исправлены metadata snapshot/generation, content equivalence, ZIP directory policy и strict display names; добавлен persisted Profile reference. | Только host prototype. Launcher/VFS/механизм resources/save format не менялись. Безопасный installer и production profile activation отсутствуют. |

Термин «закрыто» здесь относится к указанному host-контракту кандидата, **не к пользовательской APK**. Confirmed source defect не означает установленную причину конкретного Android вылета. Новые MM defects относятся к прежнему offline прототипу, а не к игровым ресурсам.

## TaskQueue: корректность и цена

D2/taskqueue/TASKQUEUE_CANDIDATE_AUDIT.md, HARDWARE_VALIDATION_PLAN.md, RESULTS_SUMMARY.json содержат детали.

- 300000 переданных задач выполнены ровно один раз в проверенных1P4C/4P4C/1P7C workloads; producer payload совпал. Оба queue-only/combined прошли по216 повторных shutdown/restart sessions с обычной/paused/nested работой на2/4/8 CPU fixtures. Протестированы captures, nested results, capacity4096 и wrap boundaries.
- В исходном настоящем TaskManager shutdown test сохранился failure: на четвёртой сессии completed123 вместо512. Это функциональное подтверждение неполного drain в host fixture, не Android crash reproduction.
- Три semantic mutations обнаружены, четыре настоящие ARM64/API24 NDK TU compilation проходят. Release Wait с acquire отдельно проверен в ARM assembly: ldrh→ldarh. ASan/UBSan не заменяют race detector. TSan не запускался повторно после D1 ошибки unexpected memory mapping; Valgrind не найден.
- Fair serial host ABBA microbenchmark: enqueue+dequeue median4.97→10.57ns, batch128:5.04→9.97ns; empty pop0.419→5.17ns. Это около2× и12× на данных тестах x86 WSL, **не Android FPS**. Рабочая нагрузка/contended ARM cost неизвестны.
- try_push(false) не меняет полную очередь. Старый void push и allocator guard завершают процесс диагностически вместо silent loss/reuse. Нужна отдельная нефункциональная политика переполнения, ownership завершённых Task* и приёмка CPU cost; без них кандидат нельзя выпускать.
- Старый D1 sequence prototype также не является готовой заменой: найден signed subtraction overflow около INTPTR_MAX под UBSan, нет drop-in full/shutdown/lifetime контракта. Этот предельный counter не приписывается сегодняшней игре.

## Архивы/сейвы: подготовленный кандидат v2

D2/decompression/DECOMPRESSION_CANDIDATE_AUDIT.md, INDEPENDENT_REVIEW.md и HARDWARE_VALIDATION_PLAN.md — полный контракт и ограничения.

- 43 valid cases,1453 rejected inputs,588 bounded mutations; семь существующих .sav backup files распакованы побайтно как прежним decoder. Все сохранённые исходные hashes неизменны. Изменения output literals могут оставаться корректным LZO: checksum semantics не придуманы.
- ASan/leaks с обычным upstream copy profile; полные ASan/UBSan host fixtures с portable-copy LZO flags. Начальное upstream unaligned-copy UBSan alignment событие сохранено и не выдано за PASS. Production LZO flags не менялись.
- 9 preview и10 ALife source-extracted cases; реальные IReader POSIX maps, по1152 repeated lifetime/error cases и100000 wide-oracle cases при FS_DEBUG on/off; шесть mutations обнаружены. CStreamReader construction в fixture — stub, его реальный map/remap этим не проверен.
- Четыре настоящие NDK TU compilation и exact in-memory patch application/reversal проходят. Полного link/APK/device не было.
- Независимое review исправило два контракта: legacy output capacity и zero-length stream before construct. Review также выявило неудачный negative fixture offset128 в архиве64: mutant не обнаруживался. Это сохранено в review-v2-test-gap; финальные тесты используют offset0/64 и обнаруживают удаление guard.

## Launcher: что добавлено к диагностике

D2/launcher/LAUNCHER_ADDITIONAL_DIAGNOSTICS.md, HARDWARE_CAPTURE_PLAN.md, RESULTS_SUMMARY.json.

- 200 быстрых завершений Game в обоих порядках destroy/resume,100 поздних callbacks и400 actual AppLog close/resume cycles проходят. Rotation/recreation — mock sequences, не Android Activity tests. AppLog уже flush-ит каждый record, лишний flush patch не добавлялся.
- Game onDestroy сейчас записывает вход, но не завершение SDL teardown. Surviving session marker может означать live Game/незавершённый teardown/reclamation/ошибку удаления, а не обязательно crash. Нужна корреляция PID/session/instance/task/sequence и Java/native/ANR/process evidence.
- Изолированный журнал: ограниченные CRC-framed JSON записи, session UUID/PID/monotonic time, два private generations, явные write/sync failure statuses, prefix-only incomplete-tail recovery. Complete corruption сохраняется, append отказывает; он не классифицирует crash.
- Восемь journal suites,800 concurrent records, независимый decoder1085 complete records; normal/forcibly terminated Windows Java children сохранили по40 acknowledged synced records. Три mutations обнаружены, SDK34 bootclasspath compilation проходит. Это не Android SIGKILL/power-loss гарантия.
- Journal не имеет production call sites. FileDescriptor.sync на lifecycle thread способен задерживать UI; пока нет Android latency budget, private export и проверенной placement policy. Не включать автоматически ради «более подробных логов».

## Независимые изменения и откат

| Набор | Файлы/варианты | Применение сейчас |
|---|---|---|
| Queue | D2/taskqueue/queue-only.patch, completion-acquire.patch, allocator-guard.patch, combined.patch | Никакое. combined включает три изменения, не применять вместе с ними ещё раз. Host reference/NDK evidence относятся к сохранённым вариантам. |
| Codec/consumers | D2/decompression/DEC-001..004.patch; candidate/ содержит6 файлов | Никакое. Consumer patches зависят от DEC-001. Есть exact preimages/hashes/roundtrip. |
| EGL | D2/egl/recovery_model.py/test_recovery_model.py, source-map.json | Только модель правил и план; engine source patch отсутствует намеренно. |
| Launcher journal | D2/launcher/patches/L06-OFFLINE-PROTOTYPE-ONLY.patch | Addition-only prototype, без hooks и APK. |
| Mods | D2/mod-foundation/patches/MM-001..004 и aggregate against D1 | Только Python reference против D1 prototype, не патчи к engine/launcher. |

Rollback на данном этапе означает не интегрировать внешний кандидат: рабочий код не изменён. Если начнётся отдельный staging, откатывать только собственный патч относительно exact preimage и проверять overlapping edits; не заменять весь файл старым архивом поверх других fixes. Не использовать reset/clean и не удалять старые артефакты.

## Следующие шаги

1. **Никаких APK до thermal restore:** процедура и точные guards в NEXT_STABILITY_SESSION.md. cordonperf1, thermala1, stabilityaudit1 сохраняются по прежним SHA256.
2. Сначала baseline ordinary Exit→Settings capture и separate decoder/link candidate acceptance. Queue требует ещё offline full/reuse policy review и ARM cost measurements; не объединять его с decoder/GL/thermal/mods.
3. EGL: сначала owner/recipe registry и standalone renderer fault harness, затем отдельный device candidate. Не вызывать старые delete в новом namespace и не подменять настоящий loss отправкой события.
4. Mods: продолжить из MOD_MANAGER_READINESS.md. До стабильного launcher и разрешения на integration — только host components. Решение расширять существующий CLocatorAPI, а не запускать параллельную VFS, сохранено.
5. Если планшета всё ещё нет, безопасное продолжение — статический/fixture audit real CStreamReader map failures, upper-layer null reader contracts, Task* ownership/full policy. Не повторять уже завершённые тесты без изменения/нового вопроса; не объявлять новые runtime результаты.

Аппаратная проверка всех кандидатов, полнота EGL recovery и реальный launcher crash — **не завершены**. Доступная offline часть сохранена так, чтобы следующий агент мог продолжить без установки экспериментальной общей APK.
