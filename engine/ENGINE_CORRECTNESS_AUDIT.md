# Native engine correctness audit — 2026-10-23

## Область и статус

Проверено текущее рабочее дерево Android-порта, включая ранее внесённые исправления SoC/освещения/AI/квестов/физики. Для этого отчёта прочитаны startup/shutdown `CApplication`, console/bootstrap arguments, save preview/ALife load, Lua VM lifecycle и общий LZO decoder. Основные изменения и тесты подготовлены агентом B; затем другой агент независимо сверил фактические исходники, before-копии, извлечённые тела функций, результаты и NDK objects.

Устройство недоступно. ADB, game launch, реальные сохранения пользователя, APK и Android-настройки не затрагивались этой проверкой. **Ни одна находка ниже не объявляется установленной причиной пользовательского вылета лаунчера после выхода из игры.** Java exception, native signal, ANR, process death и crash marker должны различаться при аппаратной приёмке.

Здесь «Confirmed» означает конкретно доказанный source-level дефект в указанной fixture. Это не утверждение, что он уже наблюдался в пользовательском сейве. Проверки с mock FS/Lua/reader не заменяют полноценную загрузку уровня на ARM64.

## Реализованные минимальные исправления

### ENG-001 — переполнение локального буфера из -ltx / -fsltx

- **Severity: High. Confidence: Confirmed для старого -ltx scan; тот же небезопасный шаблон -fsltx устранён общим helper.**
- Исходники: `src/xrEngine/x_ray.cpp::InitConsole`, `CApplication::CApplication`, новый локальный template `ReadCommandLineToken`.
- До правки `%[^ ]` без ограничения ширины писал token в `string64 c_name` или `string_path fsgame`. Fixture извлекает активное старое выражение scanf; token из 4096 символов воспроизводит ASan stack-buffer-overflow. Пустой suffix также не давал гарантированно инициализированного результата старому `c_name`.
- Минимальное исправление: определить длину до **того же литерального пробела**, проверить `0 < length < capacity`, копировать ровно length bytes и NUL. Пустое/слишком длинное значение отклоняется с log; `-ltx` оставляет user.ltx, `-fsltx` — default FS configuration. Это не новая система аргументов.
- Проверка: actual helper, capacities 64/520/8192, длины около границ, защитные canaries и legacy whitespace cases, ASan/UBSan. Mutation `length >= Size` → `length > Size` обнаруживается тестом.
- Сохранённое поведение: валидные имена копируются без изменения; исходный space-only delimiter сохранён, tabs/newlines специально не превращены в новые разделители. Поддержка кавычек/пробелов в пути не добавлялась.
- Риск регрессии низкий для обычных валидных имён; на устройстве нужны startup с default config и отдельным валидным `-ltx`. Слишком длинные значения теперь отклоняются вместо переполнения.
- Изолированный review patch: `../stability-20261023/engine/ENG-001.review.patch`.

### ENG-002 — усечённый заголовок сейва проходил предварительную проверку

- **Severity: High. Confidence: Confirmed — actual-source header fixture + ASan.**
- Исходники: `src/xrGame/saved_game_wrapper.cpp::CSavedGameWrapper::valid_saved_game(IReader&)`; callers — preview constructor и `CALifeStorageManager::load(LPCSTR)`.
- Старый guard проверял только 8 bytes (magic/version). Оба callers после успеха читают третье u32 `source_count` и считают compressed payload от offset 12. Файл ровно с двумя корректными словами проходил validator и давал read за пределами буфера при чтении третьего.
- Исправление: отклонять `length <= 3 * sizeof(u32)` перед любыми reads. Таким образом нужны все три слова и хотя бы один byte payload. Magic и условие версии `version >= ALIFE_VERSION` не менялись.
- Проверка: extracted validator; валидная 16-byte fixture сохраняет `source_size=4`, 8-byte truncated fixture до правки даёт ASan heap-buffer-overflow, после отклоняется без чтения третьего слова. Реальный IReader заменён минимальным recording reader.
- Риск низкий для нормального LZO save: корректный compressed stream не является пустым. Это **только минимальная проверка заголовка**, а не доказательство валидности compressed payload, source_count, chunks, graph IDs или всех сохранений.
- Review patch: `../stability-20261023/engine/ENG-002.review.patch`.

### ENG-003 — preview буфер терялся на существующих ранних возвратах; r_open мог вернуть null

- **Severity: Medium для утечки, High для null dereference. Confidence: Confirmed — actual-source constructor/validator fixtures.**
- Исходники: `saved_game_wrapper.cpp::CSavedGameWrapper::CSavedGameWrapper(LPCSTR)`, `valid_saved_game(LPCSTR)`.
- Preview выделял распакованный `source_data` и освобождал его только в последней строке. Существующие returns при отсутствии subchunk/spawn/graph обходили эту строку. В fixture это 65536 bytes на неуспешный preview; размер в настоящих сейвах зависит от файла и здесь не измерен.
- `FS.exist` до `r_open` не гарантирует успешного открытия; оба вызова validator/constructor раньше разыменовывали null reader. Fixture сценария failed open воспроизводила ошибку и для constructor, и для static validator.
- Исправление: `source_data` переведён в `std::unique_ptr<void, deleter>` с прежним `xr_free`; фактические consumers получают `.get()`. Added null guards перед validator; `r_close` вызывается лишь для существующего stream. Нормальный decode/parse и прежние fallback поля сохранены.
- Важная граница: manual `F_entity_Destroy` и close chunk/spawn на этих returns **уже существовали в before-копии**; они не приписываются этому патчу.
- Fixture проверяет: success; missing spawn subchunk 0; missing spawn file; failed spawn open; missing graph/fallback; failed initial open; invalid validator; static-validator failed open. После каждого исходные counters allocations/readers/objects равны 0, ASan/UBSan/leak checking проходят.
- Lifetime проверен отдельно статически: owning buffer объявлен до stack IReader; IReader и graph consumers завершаются до deleter. Двойной final xr_free удалён. Ранние returns не используют освобождённый buffer.
- Риск низкий для валидных preview; тесты моделируют декомпрессию и graph/actor данными-заглушками. Они не проверяют настоящую сериализацию, реальный game.graph, Lua callbacks или произвольный повреждённый actor chunk.
- Review patch: `../stability-20261023/engine/ENG-003.review.patch`.

### ENG-004 — reader не закрывался при отклонённом save header

- **Severity: Medium. Confidence: Confirmed — source branch fixture / reader accounting.**
- Исходники: `src/xrGame/alife_storage_manager.cpp::CALifeStorageManager::load(LPCSTR)`, ветка `if (!gameSaveIsValid)`.
- При успешно открытом, но не прошедшем validator сейве функция восстанавливала m_save_name и возвращала false, не закрывая stream. Повторные попытки могли удерживать reader/file resources. В debug перед этой веткой есть VERIFY3; если он завершает процесс, ветка не достигается — это не отменяет release ownership defect.
- Исправление ровно одной строкой: `FS.r_close(stream)` перед прежним восстановлением имени и return. Ни save format, ни unload/reload levels, ни callbacks не менялись.
- Fixture извлекает реальный блок, выполняет valid и invalid branches и проверяет нулевой reader counter. Baseline assertion ловит утечку, after PASS. Mock counter не является измерением реальных Android file descriptors.
- Риск низкий; нужны реальные rejected-save UI flow и последующая загрузка валидного сейва.
- Review patch: `../stability-20261023/engine/ENG-004.review.patch`.

## Другие подтверждённые дефекты и их текущий статус

### ENG-005 — rtc_decompress использует unsafe LZO decoder; договор об ошибках не завершён

- **Severity: High. Confidence: Confirmed — actual decoder ASan reproduction на malformed input.**
- `src/xrCore/Compression/rt_compressor.cpp::rtc_decompress` вызывает `lzo1x_decompress`, а не safe decoder. Возвращаемый статус только логируется/VERIFY; callers не проверяют фактический распакованный размер перед разбором данных.
- Исходный существующий test с однобайтным повреждённым payload и реальным `oxr_deps/lzo-src/src/lzo1x_d1.c` показывает ASan heap-buffer-overflow READ на границе input buffer. Это не модель переписанного LZO и не дефект GL/ANGLE. Новых malformed-input запусков при независимом review не выполнялось.
- Основные consumers: save preview, `CALifeStorageManager::load(LPCSTR)` и архивный `CLocatorAPI::r_open`. Global замена decoder влияет и на `.db*`, не только на сейвы.
- `ENG-002` не защищает от такого payload: наличие 12-byte header и непустого остатка не доказывает корректность LZO stream. Недостоверный source_count также может привести к чрезмерной allocation до decode.
- **Не изменено.** Недостаточно поменять имя decoder на `_safe` и вернуть 0 при ошибке: нынешние callers могут продолжить parse source_count bytes независимо от return. Нужен явный failure contract, propagation/cleanup в каждом consumer, ограничения размеров и тесты валидных archives/saves, truncated input, output overflow и corruption.
- Отдельный этап должен определить, когда отклонять файл без разрушения текущего уровня: ALife load сейчас делает unload/reload до decode. Спонтанно изменять порядок загрузки и rollback большого графа объектов нельзя.
- Свидетельства: `baseline-lzo-malformed-run-default.stderr`, `fixed-lzo-malformed-run-default.stderr`, `after-regressions.json` в evidence directory. Слово `fixed` здесь — исторический prefix набора тестов, **не** утверждение об исправлении LZO.

### ENG-006 — повторный Lua reinit теряет прежний scriptBuffer

- **Severity: Medium. Confidence: Confirmed — extracted lifecycle / LeakSanitizer.**
- `src/xrScriptEngine/script_engine.cpp::CScriptEngine::reinit` при каждой успешной VM creation выделяет новый 1 MiB scriptBuffer без освобождения предыдущего; destructor освобождает только последний указатель.
- Fixture с исходными reinit/destructor и mock Lua state после первой и восьми повторных reinit воспроизводит 8 MiB в 8 потерянных allocations. Это точный размер **этого теста**, не измеренное потребление Android процесса.
- **Реализован координатором отдельный ENG006 patch:** `xr_free(scriptBuffer)` перед новой allocation после успешного VM creation. Порядок VM callbacks, luaL_newstate, standard libraries и exporter не менялся. При ошибке VM creation прежний buffer остаётся owned и освобождается destructor либо следующим успешным reinit.
- Final actual-source fixture проходит ASan/UBSan/leaks; отдельный controlled-address test проверяет 32 reinit с разными адресами и 32 с повторным адресом, а также VM-allocation failure/retry. Buffer/VM counters в конце нулевые. Это mock Lua allocator, не измерение реальной LuaJIT памяти.
- Финальный стабильный source hash при извлечении/исполнении/NDK: `ce06cc9880217796169a05be1d5912682ec387b5f1943338e93f81a6f04317b2`. Патчи координатора — `../stability-20261023/root-fixes/ENG006*`, `ENG007*`. Аппаратная приёмка pending.

### ENG-007 — main Lua state остаётся в stateMap после destructor

- **Severity: High как dangling registry ownership. Confidence: Confirmed — extracted map lifecycle; реальный UAF не воспроизводился.**
- `CScriptEngine::~CScriptEngine` закрывает VM без `UnregisterState(m_virtual_machine)`, хотя reinit использует UnregisterState. В static stateMap остаётся `(закрытый lua_State*, уничтоженный CScriptEngine*)`.
- Fixture подтверждает: после destructor registry size=1; GetInstance старого key возвращает старый engine pointer; повторная регистрация того же адреса новым engine отвергается. Ни один тест не выдаётся за доказательство реального обращения game callback к уже освобождённому engine.
- **Реализован координатором отдельный ENG007 patch:** destructor после lua_close вызывает UnregisterState для main VM, аналогично прежнему reinit. Порядок process/profiler shutdown не переставлялся. Три исходных modes, controlled address reuse и передача того же ключа следующему engine проходят ASan/UBSan/leaks; NDK compilation PASS.
- **Оставшаяся граница:** патч удаляет main-state key, но не выполняет sweep всех keys по engine pointer. Дополнительный тест намеренно регистрирует независимый unmanaged coroutine key без CScriptThread/process owner: после destructor этот key остаётся, после чего fixture явно его очищает. Это подтверждение границы двух минимальных патчей, не доказательство остаточных keys при нормальном managed process teardown. Настоящие CreateScriptThread/DestroyScriptThread и LuaJIT coroutines требуют отдельного integration test.
- Нельзя объявлять устранёнными любые Lua UAF или полностью очищенную coroutine registry только по успешным main-state tests. Hardware/LuaJIT validation pending; scheduler не переписывался.

### Дополнительные подозрения (не принятые runtime bugs)

- **Medium / Strong Evidence:** `CLocatorAPI::_initialize` обрабатывает `-overlaypath` через unbounded `%[^ ]` в `string1024`. Guard ищет вариант без пробела, а следующий strstr — вариант с пробелом; suffix без пробела может дать null arithmetic. Это отдельная зона FS/config audit, не покрытая ENG-001. Нового reproducer для неё в этой проверке нет.
- **Medium / Hypothesis:** более глубокий повреждённый save может дать неверный actor type/graph ID/chunk length после поверхностного header check. Release VERIFY не является полноценной валидацией. Нет конкретной новой runtime репродукции; не добавлялись произвольные fallback objects или изменения игровой логики.
- **Не объявлено багом:** destructor Lua закрывает VM до удаления script processes, но в текущем `script_thread.hpp` определён `LUABIND_HAS_BUGS_WITH_LUA_THREADS`. Потенциально опасный `luaL_unref` branch в DestroyScriptThread поэтому выключен; нельзя заявлять UAF по одному порядку вызовов без учёта этой конфигурации.
- Unbounded scanf в `_EDITOR -wf`/desktop tools не входит в Android APK; это не подтверждённый Android startup crash.

## Проверка нормального startup/shutdown и прежнего поведения

Прочитаны `xr_3da/entry_point.cpp`, `x_ray.cpp`, `xrCore.cpp`, `Engine.cpp`, `IGame_Persistent.cpp`, renderer reset/destroy и Lua process/thread teardown.

- `CApplication::Run` вызывает `Device.Shutdown` (seqAppEnd), destructor вызывает OnAppEnd/destroy_persistent, detach input/settings, console cfg_save, sound/render/device/Engine destroy, Core destroy и SDL_Quit. Эти этапы данным набором native fixes не переставлялись.
- Game persistent снимает собственные frame/activate/deactivate callbacks и kernel handlers. Этого недостаточно, чтобы доказать отсутствие любых observers во всём движке; конкретных новых повреждённых references на этом пути не подтверждено.
- Input/sound startup tasks ставятся до Core.Initialize, но worker threads запускаются внутри Core.Initialize **после** присвоения Core.Params. Сам факт расположения AddTask выше Core.Initialize не использован как доказательство null Params crash.
- Renderer reset синхронизирует r_main/r_sun tasks; полное scheduler race/termination исследуется отдельной зоной concurrency audit. Из этого отчёта scheduler не менялся.
- Никаких изменений budgets AI/Lua scripts, physics time step, квестов, save serialization, рендера или качества изображения для «устойчивости» не делалось.

## Реальные проверки и ограничения результатов

Evidence directory: `../stability-20261023/engine/`.

| Проверка | Подтверждённый результат | Ограничение |
|---|---|---|
| Старый command scan, 4096-char token | ASan stack-buffer-overflow | Контроль before-expression, не новый helper |
| Новый actual token helper | ASan/UBSan + canaries PASS; mutation обнаружена | Не новый shell/parser и не Android UI |
| Actual save header | Normal PASS; прежний truncated OOB больше не вызывается | Mock IReader, не валидатор LZO |
| Actual preview constructor/validator | Все 8 selected scenarios: allocations/readers/objects=0, ASan/UBSan PASS | Decompression/graph/actor/FS mocked |
| Invalid-save reader branch | Before assertion, after PASS | Не Android FD trace |
| Actual unsafe LZO | ASan OOB по malformed input сохраняется | **Отложенная проблема**, не PASS оптимизированного decoder |
| Baseline Lua lifecycle | 8 MiB leak; stale registry/address reuse detected | Lua VM/profiler/map lock mocked |
| Final actual Lua lifecycle, 3 modes | Все returncodes 0, ASan/UBSan/leaks PASS | Source hash проверен до и после, VM mocked |
| Lua controlled-address lifecycle | 32 distinct + 32 reused addresses, cross-engine key reuse, VM allocation failure/retry PASS | Не реальное распределение адресов LuaJIT |
| Unmanaged coroutine registry boundary | Main key удалён; независимый orphan key остаётся до explicit unregister | Оставшееся ограничение, не полное managed teardown |
| Реальные Android24 ARM64 NDK27 Release TU | x_ray.cpp, saved_game_wrapper.cpp, alife_storage_manager.cpp: returncode 0, objects созданы | Compile, не link/APK/устройство |
| Final actual Lua NDK TU | script_engine.cpp, returncode 0, object 775556 bytes | Compile, не реальная VM/game acceptance |
| Независимый повторный review | 3 source hashes, 5 фактических extracted method fragments и 3 object/log пары совпадают | Сам review ничего не исполнял |

В `after-regressions.json` 23 build/run records. Три ненулевых исхода **ожидаемые**: старый control command-scan oversized, mutation нового helper и unchanged malformed LZO. Нельзя представлять этот JSON как «всё прошло» без такого разделения. Фактические проверки ENG-001/002/003/004 проходят; ENG-005 по-прежнему выявляется.

NDK objects и журналы существуют:

```
x_ray.cpp.ndk.o                 400512 bytes
saved_game_wrapper.cpp.ndk.o    650680 bytes
alife_storage_manager.cpp.ndk.o 501840 bytes
script_engine.cpp.ndk.o         775556 bytes
```

Точные команды, paths/defines, elapsed и returncodes — `ndk-results.json`, для Lua — `lua-ndk-results.json`; сборка без исходного PCH. В game TUs есть compiler warnings из существующих headers; нулевой return code не означает отсутствия предупреждений.

Исходный неуспешный `lua-fixed-results.json` был запущен до применения root patches и **не используется** как их final validation. До повторного запуска его JSON, generated source, binary и журналы скопированы без изменений в `lua-pre-root-invalid/` с `VALIDITY_NOTICE.json`. Новая принятая запись проверена wrapper `validate_root_lua.py`: production SHA совпадает с ожидаемым до и после run, все 4 compile/run outcomes нулевые; `root-lua-verification.json` фиксирует hash generated fixture. Дополнительные controlled allocator cases и ограничение coroutine registry — `lua-address-results.json` / `lua-address-run.log`.

Исходные raw SHA256 после этих четырёх логических исправлений:

```
src/xrEngine/x_ray.cpp               f293c9b1d1272a7c451d4de8b0619a5e606d6972f5b14d36cbdae2c4e4115ced
src/xrGame/saved_game_wrapper.cpp    2e1b3c1db55b7314c00ff959c4313f57ede455e1cd00c1170aece066310b8da0
src/xrGame/alife_storage_manager.cpp 27e78cec2f905d7f983adc4433452faffd99634de3b3eb4dbce3dc44e251b36d
src/xrCore/Compression/rt_compressor.cpp (unchanged)
                                    db88898bf3d1e72d522808ab3bb7020ed5a19bbffb0c46ed58b18ac418f36cb0
src/xrScriptEngine/script_engine.cpp (final ENG006/007)
                                    ce06cc9880217796169a05be1d5912682ec387b5f1943338e93f81a6f04317b2
```

CRLF существующих production файлов сохранён. Отдельные `ENG-*.review.patch` получены сравнением точных before/current; ENG-002/003 разделяют два независимых изменения одного saved_game_wrapper.cpp, без переписывания production ради документации. `independent-review.json` фиксирует **исторический checkpoint первых трёх файлов** до root Lua/shutdown-trace изменений, а не полный финальный snapshot.

После этого checkpoint координатор добавил FT_SHUTDOWN instrumentation в CApplication destructor; текущий x_ray.cpp hash `a4717dbb7ee54c2586f9ec1c0b3be98f056cae9d6e48db603b10d74df83f4662`. Исторический hash f293… и первый x_ray NDK object выше относятся к ENG-001 до этой instrumentation; её отдельную final compile/verification выполняет concurrency/root зона. Они не подменяются задним числом. Lua final compile и fixtures уже относятся к final ce06… source и имеют отдельные hash guards.

## Следующие действия

1. Проверить принятые минимальные Lua lifecycle fixes в реальном LuaJIT/game lifecycle; отдельно изучить unmanaged/coroutine ownership. Offline main-state/buffer проверки завершены, вся подсистема Lua не объявлена принятой.
2. Подготовить failure contract безопасной декомпрессии как отдельный изолированный этап. Начать с полного списка callers и fixtures валидных saves/archives; не ограничиться заменой одной функции LZO.
3. После thermal restore и разрешённой установки отдельного stability candidate проверить valid save load/preview, новый save и повторную загрузку, переходы menu ↔ level, штатный выход/повторный запуск, Native и ANGLE. Исходные saves не изменять — использовать копии.
4. Launcher crash расследовать самостоятельно по Android/logcat/exit-info evidence; изменения native argument/save cleanup не являются его доказанным исправлением.

**Статус:** четыре native argument/save fixes и два минимальных Lua buffer/main-registry fixes реализованы и имеют offline evidence; аппаратная приёмка pending. Уровни/AI/физика/Lua runtime не объявлены полностью проверенными. Unsafe LZO failure contract и остаточный unmanaged coroutine ownership отделены от уже выполненных исправлений.
