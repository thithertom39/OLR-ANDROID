# Готовность изолированной основы менеджера модов — 23.10.2026

**Host-reference расширен и проверен; production-интеграция не выполнялась и пока не готова.** Нет Android UI, установщика, mount hooks или второй runtime VFS. Python не включён в APK. Launcher, CLocatorAPI, формат сейвов, game-root, Gradle, зависимости и защищённые APK не менялись этим направлением.

## Исходное состояние и границы

- R: `C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/openxray-build` — исходники только прочитаны.
- D1: `C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/stability-20261023/mod-foundation` — прежний прототип сохранён без изменений.
- D2: `C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/stability-followup-20261023/mod-foundation` — новые кандидаты/тесты/отчёт.
- D1 model SHA256: `8c1788fa4e96a93192d0099377672269eb85d9f07892cfc0d82519878ba33fe3`.
- D2 model SHA256: `48a51bfdf24ca94e2421fa5121b0557f2c30f99ec6c95fd344109bf8e612804d`.
- Все 28 прежних методов `test_mod_foundation.py` перенесены **без изменения байтов**; SHA256 `c41876e40556573a916396780b8ee7e1300c99d3522b068ebd12fa128d868573`.

Прочитаны R/MOD_MANAGER_ARCHITECTURE.md, R/NEXT_STABILITY_SESSION.md, D1 implementation/tests/results. Дополнительно сверены LocatorAPI.cpp::LoadArchive, unload_archive, r_open_impl и OpenXRayActivity.java::extractAssetFolder/installFsgameConfig. Сохранено решение расширять существующий locator, не подменять получение game bytes поиском Python/Java.

## Подтверждённые host-дефекты и кандидаты

| ID | Severity / confidence | Наблюдение D1 и границы вывода | Исправление и тест |
|---|---|---|---|
| MM-001a | Medium / Confirmed host behavior | FileResolver проверяет entries detector-ом, затем повторно проходит для индекса. Nested generator исчерпан: resolve возвращает None. D1 annotation требует tuple для entries; runtime границу не валидирует. Штатный inspect_zip возвращает tuple. Это **не потеря игровых ресурсов в APK**. | _freeze_layers материализует вложенные iterables один раз, проверяет provider и FileEntry. Snapshot используется для проверки и индекса. One-shot iterable и внешняя mutable collection покрыты тестами. |
| MM-001b | Low / Confirmed metadata-key collision | generation учитывала только winners/hash; изменения размера, затенённого содержимого, порядка нижних одинаковых слоёв или пустого слоя могли не менять ключ. D1 cache локален resolver-у, поэтому stale cache между экземплярами не воспроизведён. | Ключ включает все слои, их порядок, path/size/hash каждого entry. Внутри слоя paths сортируются. Покрыты shadowed/empty/order изменения и стабильность при перестановке entries. |
| MM-001c | Low / Confirmed metadata behavior | identical_content сравнивал лишь digest при различающемся размере. Это противоречивый metadata index, **не SHA256 collision**. | Сравниваются size и digest. Происхождение всех providers сохраняется. Вне inspect_zip реальные байты не проверяются. |
| MM-002 | Medium / Confirmed dry-run validation gap | ZIP-directory переходил continue до проверки размеров/чтения. Directory payload 20000 bytes проходил при file_bytes=1024/total_bytes=2048. Байты не распаковывались: это пропуск policy, не доказанный OOM. Принимался S_IFREG с именем, оканчивающимся на /. | Отклонены ненулевой directory payload и противоречивый mode/name. Empty stored/deflate directories остаются совместимы; compressed size у deflated empty может быть ненулевым. |
| MM-004 | Low / Confirmed parser behavior | JSON escape lone surrogate/DEL принимался в display name. Lone surrogate не кодируется strict UTF-8 позже; Android crash не воспроизводился. | Отклонены surrogate range и DEL. Обычный Unicode, включая русское имя, разрешён. |

Все эти дефекты относятся к **D1-прототипу**, не к production launcher/движку. Их нельзя смешивать с TaskQueue, rtc_decompress или установленной причиной пользовательского вылета.

## Новая возможность — persisted Profile reference

MM-003 добавляет Profile.parse(bytes) и Profile.to_json(): **новый host API**, а не исправление отсутствовавшей функции. Пример:

```json
{"schema_version":1,"profile_id":"example","mods":[{"id":"base","version":"1.0.0"},{"id":"overlay","version":"2.3.4"}]}
```

Лимиты: 64 KiB, 128 pins. Проверяются точные поля/типы, schema 1, безопасный id, явные version triples. Отклонены duplicate JSON keys на любом уровне, неверный UTF-8, nonfinite numbers, неизвестные поля, повтор mod id и wildcard/range версии. Порядок pins сохраняется; зависимости автоматически не сортируются. Canonical roundtrip покрывает пустой и максимальный профили.

После parsing **отдельно нужен ProfileManager.validate**: installed inventory, compatibility, dependencies/ranges/cycles/conflicts/load order. Parser не доказывает существование payload. to_json возвращает bytes, не записывает/активирует профиль и не меняет aliases/saves. Atomic persistence, activation journal, rollback и связь версии с immutable payload digest отсутствуют.

## Проверки и воспроизведение

- tests-windows.json/.log: **49 методов PASS**, Python 3.12.14 / Windows 11.
- tests-wsl.json/.log: **49 методов PASS**, Python 3.12.3 / Ubuntu WSL2.
- Это **28 прежних + 21 новый** метод, а не 49 новых проблем. Старый deterministic corpus из 2000 путей сохранён.
- tests-windows-baseline.json/.log: новые проверки сначала выполнены против неизменённого D1. Ошибки отсутствующих Profile.parse/to_json в baseline — новый API, не старый runtime crash. Поле base_source_sha256 фиксирует импортированный D1 отдельно от local sources.
- mutation-results.json и mutation-results/*.log: **5/5 намеренных регрессий обнаружены**: iterator reuse, hash-only equivalence, пропуск directory bytes, duplicate-key profile parsing, lone-surrogate name. Faults только во временных копиях.
- candidate-manifest.json, evidence-sha256.json: preimage/candidate/patch/test/report hashes.

```powershell
$taskPython = 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$taskMods = 'C:/Users/User/Documents/Codex/2026-10-11/gjvyb/work/stability-followup-20261023/mod-foundation'
& $taskPython -X utf8 "$taskMods/run_followup_tests.py" --label windows
wsl -d Ubuntu-24.04 -- python3 /mnt/c/Users/User/Documents/Codex/2026-10-11/gjvyb/work/stability-followup-20261023/mod-foundation/run_followup_tests.py --label wsl
& $taskPython -X utf8 "$taskMods/run_mutations.py"
& $taskPython -X utf8 "$taskMods/verify_candidate.py"
```

prepare_followup.py воспроизводит четыре патча из точного D1 preimage и отказывается перезаписывать уже изменённый candidate. Для review: patches/MM-001..004; all-followup.patch — эквивалентная общая host-only разница против D1. Патчи не предназначены для применения в production R. verify_candidate.py проверяет их replay в памяти, не изменяя D1/D2.

## Готовность и оставшиеся ограничения

| Компонент | Доказанное состояние | Следующий gate |
|---|---|---|
| Manifest, dependencies, load order | Host tests PASS в ограниченной схеме | Нет SemVer prerelease. JSON declaration не доказывает совместимость scripts/textures/shaders. |
| Profile serialization | Bounded canonical host roundtrip | Нет atomic persistence, journal, rollback, UI, экспорта/migration saves. |
| Resolver/conflicts | Metadata snapshot/provenance/last-layer wins | Не открывает game bytes, не защищает физический диск от TOCTOU. Не заменяет CLocatorAPI. Не shader-cache key. |
| ZIP checking | Bounded dry run + hashes, directory cases закрыты | **Safe installer отсутствует**: нет extraction, SAF, immutable quarantine, disk budget, cancellation, no-follow writes, durable transactions/recovery, atomic publication. |
| Native .db* | Намеренно запрещены в import model | Decoder candidate другого направления не доказывает защиту всех archive parsers и не установлен в APK. |
| Layered engine providers | Архитектура D1 сохранена | Нет реализации; сначала read-only actual-index export/precedence fixtures, затем расширение существующего locator. |
| Android integration | Отсутствует | Сначала аппаратная приёмка launcher после thermal restore, затем отдельное разрешение на интеграцию. |

## Дальнейший порядок

1. Core stability candidates принимать отдельно. Не смешивать Python foundation с экспериментальной APK.
2. При возвращении устройства root-оператор сначала выполняет незавершённую thermal restore из основного handoff; далее normal-exit/Settings циклы обоих backend.
3. Согласовать protected compatibility layer: extractAssetFolder/installFsgameConfig сегодня могут заменять shader/script/config assets. User mods нельзя silently overwrite.
4. После разрешения — read-only export из **существующего CLocatorAPI**. Проверить пустой профиль против vanilla, providers/winners, case sensitivity, string256 physical-path limits. Не создавать параллельную VFS.
5. До установщика — filesystem transaction model и fault fixtures: quarantine snapshot, payload hashes, symlink запрет, disk-full/cancellation/process death, atomic publication/recovery. ZIP dry-run не заменяет эти задачи.
6. Затем небольшой texture-only fixture на устройстве с отдельными profile saves/config, Native и ANGLE, повторными start/exit и возвратом к vanilla. Shader/script fixtures требуют отдельных проверок.

Аппаратных результатов менеджера нет. Host PASS не доказывает безопасность исполнения чужого Lua, совместимость ресурсов/сейвов или стабильность Android.
