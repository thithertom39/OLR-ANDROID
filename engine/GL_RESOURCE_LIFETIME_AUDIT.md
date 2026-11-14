# GL resource lifetime audit — 2026-10-23

## Границы доказательств

Аудит выполнен по текущему рабочему дереву Android-форка, а не по чистому upstream. Прочитаны `CONTRIBUTING.md`, предыдущий `ANDROID_THERMAL_POWER_AUDIT.md`, создание/удаление RT, текстур, буферов, FBO, шейдеров, resource-manager reset, Android SDL background/foreground и engine event loop. Устройство в этой сессии недоступно; ADB не вызывался. Подтверждённый source-level дефект ниже не равен подтверждённому вылету на планшете.

Рабочие Native GLES и ANGLE → Vulkan используют одни `CRT`, `CTexture`, GLAD dispatch и GL resource wrappers. Правки этих wrappers применимы к обоим путям. Ни SDL, ни ANGLE, ни шейдеры, ни графические настройки не изменялись этим агентом. Не создавалась новая APK; аппаратная приёмка отдельных кандидатов остаётся за общей сессией.

Доказательства: `../stability-20261023/gl/` относительно корня проекта. Там сохранены исходные `.before`, отдельные патчи, source-extracted fixtures, команды компиляторов, журналы и JSON результатов. Базовая копия всей сессии создавалась координатором до разрешения правок.

## Реальный lifecycle ресурсов

1. `glHW.cpp::CHW::CreateDevice` создаёт SDL GLES context, делает его current, загружает GLAD через `SDL_GL_GetProcAddress`, проверяет выбранный ANGLE backend по живому renderer и создаёт `pFB` через `UpdateViews`.
2. `ResourceManager_Resources.cpp::_CreateRT` регистрирует `CRT` по имени. `glSH_RT.cpp::CRT::create` создаёт texture name/storage и связанный `CTexture` wrapper; `pZRT` является алиасом того же `pRT`, а не вторым GL-объектом.
3. `glSH_Texture.cpp::CTexture::Load` загружает обычные текстуры, видеотекстуры либо `.seq`; `surface_set` устанавливает GLuint без подсчёта GL references. Ряд `$user$` wrappers заимствует ресурс; его владелец должен отсоединить wrapper до освобождения.
4. `CRender::reset_begin` в `xrRender_R2/r2.cpp` сначала синхронизирует render tasks, затем вызывает `Resources->reset_begin`, после чего удаляет `Target`. Это важно: surviving RT проходят `reset_begin/end`, а часть RT уничтожается вместе с `Target` и создаётся заново.
5. `D3DXRenderBase::Reset` выполняет renderer reset_begin → `CHW::Reset` → resource-manager reset_end → renderer reset_end. `CHW::Reset` пересоздаёт FBO, но **не создаёт новый EGL context и не восстанавливает все статические GL ресурсы**.
6. `CResourceManager::reset_end` восстанавливает dynamic streams, remap geometry handles, RT в исходном порядке и state blocks. `DeferredUnload/DeferredUpload` отдельно вызывает `CTexture::Unload/Load` для зарегистрированных текстур, при OGL последовательно, без параллельных GL worker contexts.
7. Нормальное уничтожение идёт через `CRenderDevice::Destroy` → renderer `OnDeviceDestroy` → resource manager → `CHW::DestroyDevice` → SDL window destroy. В `DestroyDevice` FBO и context после удаления уже обнуляются; physical-state и texture-binding caches инвалидируются.

### Кэши, где защита уже существует

- `glTextureBindings.h::DeleteTextures` передаёт GL const pointer: GL не меняет scalar владельца. Helper обнуляет совпадающие binding-cache entries и инвалидирует attachments; это не замена обнулению `CRT::pRT` или `CTexture::pSurface`.
- `glBackendStateCache.h` различает invalid и valid zero; состояние thread-local. Общая модель соответствует единственному primary GL context, поддерживаемому `CHW::MakeContextCurrent`.
- `glState::Release` удаляет samplers через helper и очищает массив.
- `glBufferUtils.cpp` staging/stream Destroy очищают device buffer handles и инвалидируют geometry. Stream upload guards, прежние безопасные map/unmap исправления и fence policy сохранены.
- `SH_Atomic.cpp::SPP::~SPP` инвалидирует program cache, `SDeclaration::~SDeclaration` — geometry cache.
- `CRenderTarget::~CRenderTarget` явно отсоединяет borrowed material/luminance/jitter wrappers до GL удаления; environment teardown также использует `surface_set(..., 0)`.
- `CHW::Present` инвалидирует framebuffer/draw-buffer/attachment proofs после прямых read/draw framebuffer binds. Его full-frame blit и clear не менялись.
- GPU profiler, если собран отдельно, удаляет queries только когда соответствующий context current; в нормальном APK instrumentation compiled out. Это не доказывает общего восстановления context.

## Найденные проблемы

### GL-001 — удалённые RT handles сохраняются в surviving CRT

- **Severity: High. Confidence: Confirmed — source-extracted regression; device scenario pending.**
- Исходники: `src/Layers/xrRenderGL/glSH_RT.cpp::CRT::create/destroy/reset_begin/reset_end`; вызов surviving reset: `src/Layers/xrRender/ResourceManager_Reset.cpp`.
- До правки `destroy` удалял GL texture, но сохранял `pRT` и `pZRT`; `create` сразу возвращался на `if (pRT) return`. Сценарий: создать RT → reset_begin → reset_end на том же CRT. Storage и wrapper не восстанавливались. Повторный destroy также мог удалить чужой ресурс, если драйвер уже повторно выдал освобождённое имя.
- Реализован минимальный независимый патч: после `DeleteTextures(1, &pRT)` добавлены `pRT = 0; pZRT = 0;`. Metadata, порядок detach/delete, format, samples, размеры и create guard не менялись.
- Проверка: actual extracted create/destroy/reset methods; reset/recreate, 4096 reset cycles, double destroy при повторном выделении имени другому владельцу, idempotent live create, surviving borrowed wrapper detach, реальный body binding-cache deletion helper.
- Риск регрессии низкий; требуется реальное `vid_restart`/смена разрешения и свет/тени на обоих backend. Не является реализацией context-loss recovery.
- Патч: `../stability-20261023/gl/GL-001.patch`.

### GL-002 — обычная/видео texture остаётся с удалённым pSurface

- **Severity: High. Confidence: Confirmed — source-extracted regression; device scenario pending.**
- Исходники: `src/Layers/xrRenderGL/glSH_Texture.cpp::CTexture::Unload/Load`; mass path: `CResourceManager::DeferredUnload/DeferredUpload`.
- До правки только sequence branch очищала `pSurface`. Для normal/video Unload вызывал DeleteTextures, но сохранял scalar. Следующий Load устанавливал bLoaded=true и возвращался на ненулевом уже удалённом имени. Второй Unload мог удалить ресурс другого владельца после name reuse.
- Независимый патч: `pSurface = 0` сразу после одиночного DeleteTextures. Sequence branch по-прежнему удаляет каждую frame texture один раз и предварительно обнуляет текущий alias. Очистка pBuffer, pTheora, delegate и loaded flag сохранена.
- Проверка: 4096 unload/reload cycles, idempotent live Load, foreign-name reuse, sequence alias/double Unload, video reload, явно отсоединённый `$user$` wrapper.
- Риск низкий для owned textures. Патч **не вводит новую ownership политику** для произвольных borrowed `surface_set`; вызывающие их код всё ещё обязан detach. Runtime unload/reload и видео надо проверить отдельно.
- Патч: `../stability-20261023/gl/GL-002.patch`.

### GL-003 — некорректный FPS .seq вызывает целочисленное деление на ноль

- **Severity: High. Confidence: Confirmed — actual-source UBSan probe с malformed assets.** Не наблюдавшийся вылет из текущих vanilla файлов.
- `glSH_Texture.cpp::CTexture::Load` использует `u32 fps = atoi(buffer); seqMSPF = 1000 / fps;`; `apply_seq` затем делит игровое время на seqMSPF. Заголовок `0` приводит к UB при Load, `1001` и `-25` дают seqMSPF=0 и UB при применении непустой sequence.
- Сценарий воспроизведён extracted full Load + apply_seq с mock FS и UBSan; валидные 25 и 1000 проходят. Результаты: `../stability-20261023/gl/malformed-asset-results.json`.
- Реализован отдельный согласованный патч `GL-003.patch`: проверка fps в диапазоне 1..1000, понятный log, закрытие reader и return на ошибке. Неправильный fps не подменяется иной скоростью. Для валидного заголовка прежняя арифметика `1000/fps` сохранена точно.
- Final source-extracted ASan/UBSan fixture проверяет все 1000 допустимых значений fps, cycled/noncycled (2000 последовательностей), 14 000 точных выборов кадров при разных временах, 5 malformed headers и закрытие reader. PASS. Более строгий parser вместо прежнего atoi отдельно должен учитывать совместимость старых `.seq`; overflow-поведение atoi для экстремально длинных чисел этой минимальной правкой не менялось.
- Важно: пустая последовательность сама по себе не доказывает modulo-by-zero — реальный `PostLoad` выбирает apply_seq только для непустого `seqDATA`. Этот случай не объявлен подтверждённым crash.

### GL-004 — GL error во время загрузки видео теряет созданный texture name

- **Severity: Medium. Confidence: Confirmed — injected error-path ownership proof.** Частота настоящих GL errors неизвестна.
- В `CTexture::Load` OGM branch при ошибке после texture creation удаляет pTheora и устанавливает pSurface=0, не удаляя созданный GL texture. Последующий Unload уже не знает имя. Android не создаёт PBO для этого пути, но GL texture остаётся утечкой в живом context.
- Probe инжектирует GL_OUT_OF_MEMORY после video texture setup, выполняет реальный Load и получает `pSurface=0`, `pTheora=null`, `live_gl_textures=1`. Это доказательство отсутствующей cleanup, **не наблюдение OOM на Adreno**.
- Реализован отдельный согласованный `GL-004.patch`: удаление pSurface через существующий texture-binding helper до обнуления имени в OGM error branch. Другие видео/desktop AVI пути и lifetime PBO не менялись.
- 256 повторных simulated OGM GL-error cleanup cycles проходят ASan/UBSan; GL live objects остаются пустыми, cached texture name обнулён, повторный Unload ничего не удаляет второй раз. Mutation без DeleteTextures воспроизводит утечку. На устройстве реальный video/error path пока не проверен.

### GL-005 — полное восстановление потерянного EGL context не реализовано

- **Severity: High как ограничение. Confidence: Confirmed — отсутствующая recovery; частота реальной потери context неизвестна.**
- В `../deps/sdl2-src/src/video/android/SDL_androidevents.c::android_egl_context_restore` SDL пытается вернуть старый context; при неудаче создаёт новый и отправляет `SDL_RENDER_DEVICE_RESET`.
- `src/xrEngine/AndroidAppLifecycle.h::DrainRenderResetDiagnostics` намеренно только логирует `full_context_recovery_unimplemented=1`. `glHW.cpp::GetDeviceState` всегда возвращает Normal. Hardware wrapper хранит прежний `m_context`; static textures, geometry, programs/VAO и caches не проходят полноценную generation-aware пересборку после SDL replacement.
- Обычное HOME/блокировка с восстановлением **того же** context и настоящая потеря context различаются. Прежний engine fix своевременно drain background events, пропускает GL reset/draw, пока SDL не вернул drawing context; этот рабочий путь сохранён.
- Потенциальные последствия настоящей потери: stale names, отсутствующая картинка, неверные bindings, ошибки при последующих удалениях. Эти последствия не воспроизводились на устройстве в этой сессии и не названы причиной вылета лаунчера.
- Не исправлять вызовом обычного `Device.Reset` вслепую: имена прежнего context нельзя удалять в новом, где те же числа могут принадлежать уже другим объектам. Нужны context-generation ownership, потеря/пересоздание **всех** GPU ресурсов и policy при невозможности recovery. Это отдельная крупная задача с hardware fault/recovery тестами.

### GL-006 — borrowed ColorMap wrappers требуют отдельной проверки ownership

- **Severity: Medium. Confidence: Strong Evidence, runtime проявление не подтверждено.**
- `ColorMapManager.cpp::UpdateTexture` копирует GLuint из cached texture в `$user$cmap*` через surface_set; `ColorMapManager.h` не объявляет destructor, отсоединяющий такие aliases. GL CTexture::Unload удаляет любой ненулевой pSurface без reference counting. В отличие от DX11 surface_set, GL surface_set не AddRef/Release.
- Возможен двойной delete либо удаление ещё нужной texture при разном lifetime owner/wrapper. Конкретное проявление зависит от дополнительных shader references и порядка уничтожения resource manager/Target; полного renderer fixture для этого нет.
- Рекомендуется сначала source-extracted ColorMap fixture с настоящими ref_texture lifetime и surviving shader refs. Возможное минимальное исправление — explicit detach обоих cmap wrappers перед member destruction, по аналогии с environment/Target. **Не вводить глобальный bUser-never-delete без анализа всех surface_set callers.**
- Не изменено. GL-002 защищает repeated Unload одного wrapper, но не решает разные wrappers, заимствовавшие одно имя.

### GL-007 — размеры отсутствующей texture могут быть неинициализированы

- **Severity: Low/Medium по месту использования. Confidence: Strong Evidence, без runtime reproduction.**
- OGL CTexture constructor не инициализирует m_width/m_height; pSurface и desc_cache изначально 0. `SH_Texture.h::desc_valid` при 0==0 пропускает desc_update, а get_Width/get_Height возвращает поля. Для отсутствующей/ещё не установленной `$user$` texture, в частности вызываемой из `D3DXRenderBase::GetImGuiTextureId`, это подозрительный путь.
- Нужны fixture actual constructor + getters + missing-texture branch либо MSan, затем минимальная инициализация размеров нулями и проверка call sites. Ни ASan, ни обычный UBSan не доказывает отсутствие uninitialized reads.
- Не изменено; не объявляется выявленным пользовательским UI crash.

## Проверки реализованных GL-001/002/003/004

| Проверка | Результат | Что не доказывает |
|---|---|---|
| Actual-source host fixture, baseline | 5 PASS, 8 ожидаемых FAIL из 13 | Нет реального GL драйвера |
| Те же 13 случаев после минимальных патчей | 13 PASS | Не измеряет визуальную идентичность на планшете |
| WSL g++13, ASan + UBSan + leak checking | 13 PASS; sanitizer diagnostics отсутствуют | Не проверяет ошибки внутри GLES/ANGLE |
| Mutation: убрать RT zeroing | 4 ожидаемых FAIL | Только чувствительность к регрессии GL-001 |
| Mutation: убрать texture zeroing | 4 ожидаемых FAIL | Только чувствительность к регрессии GL-002 |
| Полный допустимый диапазон sequence FPS | 2000 последовательностей, 14 000 точных frame choices; ASan/UBSan PASS | GL/FS mocked, не проверяет реальный decoder |
| Invalid sequence и reader cleanup | 5 headers безопасно отклонены; ASan/UBSan PASS | Не валидирует любой возможный повреждённый asset |
| OGM GL error cleanup | 256 cycles PASS; mutation без delete даёт ожидаемый FAIL | Не является реальным OOM на планшете |
| Реальный Android NDK27/API24 ARM64, full TU | glSH_RT/glSH_Texture, diagnostics OFF/ON: 4 PASS | Syntax-only, не link/APK/device acceptance |
| Normal preprocessing | Profiler calls compiled out | Не измеряет driver overhead |

Fixture извлекает **полные** production CRT create/destroy/reset и CTexture Load/Unload, а также Android body `GLTextureBindings::DeleteTextures`. Моделируются GL allocation/deletion с немедленным reuse имён, FS, decoder, reference wrapper и небольшой binding table (2 units × 2 targets вместо полной production таблицы). Библиотека/драйвер GL не загружается. При baseline failures mock state между кейсами принудительно очищается тестовым harness; это не production recovery.

Raw evidence:

- `../stability-20261023/gl/handle-fixes.json`: исходные и конечные **raw-byte SHA256**, +29 и +19 bytes, CRLF сохранён.
- `asset-fixes.json`: отдельные GL-003/004, +207 и +66 bytes. Итоговая цепочка исходников включает все четыре патча.
- `manifest_before.json`, `manifest_after.json`: hashes извлечённых/нормализованных методов и generated fixture.
- `host-results.json`, `before-run.log`, `after-run.log`, `after-asan-run.log`, `mutation-*-run.log`.
- `ndk-results.json`, `glSH_RT.cpp-diag*-ndk.log`, `glSH_Texture.cpp-diag*-ndk.log`: точные compile commands с disabled PCH.
- `probe_malformed_assets.py`, `malformed-asset-results.json`, `asset-*.log`: дополнительные дефекты, не вошедшие в основной двухстрочный набор cleanup.
- `check_asset_contract.py`, `asset-contract-results.json`, `asset-contract-asan-run.log`: проверки реализованных GL-003/004. Исходный `probe_malformed_assets.py` — доказательство **до** этих двух правок; повторять как актуальный after-test нельзя, у него сохранены ожидаемые pre-fix ошибки.

Конечные raw SHA256 реализованных файлов:

```
glSH_RT.cpp      e735b7de1ad88457351a1d7ad90f99c7d05c2120ada445a7b2ced107eb9980cb
glSH_Texture.cpp 5695ee503c282619a11b678056701616cbd7027333ee6c314e1b8865d55b720b
```

## Аппаратная приёмка (pending)

Сначала завершить предусмотренное thermal-session восстановление конфигов, launcher preferences и обычной скорости игрового времени. Не устанавливать stability APK поверх незавершённого состояния тестов до этого восстановления. Оригиналы cordonperf1/thermala1 сохранить.

После отдельной сборки с GL cleanup выполнить на Native и ANGLE:

1. Launcher → load тот же сейв → world; проверить освещение, flashlight shadows, postprocess, foliage/AA, UI, видео в меню.
2. Несколько `vid_restart`/переключений разрешения с возвратом исходного, всегда закрывая консоль ESC. Проверить, что кадр покрывает весь drawable и RT/storage создаются заново без неполных FBO.
3. HOME, Recents, lock/unlock с паузой между циклами; сопоставить SDL lifecycle и engine diagnostic events. Если есть SDL_RENDER_DEVICE_RESET, зафиксировать отдельно; обычный успешный resume не выдавать за full-context-loss test.
4. Многократно штатно выйти к лаунчеру, изменить настройку, повторно запустить игру. Для known launcher crash сохранить Java exception/native tombstone/ANR/process transition независимо от GL cleanup результата.
5. Сравнить настройки и hashes сохранений; проверить ошибки GL только диагностическим opt-in режимом, без включения тяжёлых hooks в обычную сборку.

**Итог:** два дефекта повторного удаления/пересоздания ресурсов, деление на ноль из malformed sequence FPS и потеря texture name на video error устранены четырьмя независимыми исходными патчами и проходят offline regression/NDK. На устройстве они пока не приняты; причина известного launcher crash этим аудитом не установлена. Полное EGL context-loss recovery и неявный borrowed ownership требуют отдельных решений, а не расширения текущего минимального набора.
