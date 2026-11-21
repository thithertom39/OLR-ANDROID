# Сборка APK OLR Android через GitHub Actions

Workflow: [Build OLR Android APK](../.github/workflows/android-apk.yml)

## Запуск с телефона

После добавления workflow в основную ветку `main`:

1. Откройте репозиторий GitHub и вкладку **Actions**.
2. Выберите **Build OLR Android APK**.
3. Нажмите **Run workflow**, выберите ветку и параметры:
   - **test_build** (по умолчанию включён): тестовое приложение с суффиксом `.dev`, чтобы не заменять обычное.
   - **gpu_profile** (по умолчанию выключен): добавляет диагностику GPU.
4. Дождитесь завершения задания **Build arm64-v8a APK**.
5. В деталях запуска скачайте артефакт **OLR-Android-arm64-<номер>** (ZIP).
6. Распакуйте ZIP на телефоне и установите `OLR-Android-arm64.apk`.

Сборка запускается на компьютере GitHub; локальный компьютер и Android Studio не требуются.
Журналы компиляции доступны непосредственно в открытом запуске GitHub Actions.
Артефакт сохраняется **7 дней**. При ошибке сборки артефакт APK не появится — сначала проверьте логи.

### Что собирается

- Java 17, Gradle 8.2, Android Gradle Plugin 8.2.2.
- Android SDK 34, NDK `27.0.12077973`, CMake `3.22.1`.
- Команда `assembleRelease` из `engine/android` с `engine/HANDOFF/vendor.init.gradle`.
- Нативные зависимости берутся из `engine/vendor/*-src` без изменения CMake проекта.
- Архитектура `arm64-v8a`.

Это облачная попытка воспроизвести существующую конфигурацию, а не подтверждение успешной сборки или работоспособности приложения на устройстве.

### Важное про подпись

Без настройки секретов каждый запуск создаёт **новый временный ключ**.
Поэтому APK из одного запуска может не установиться поверх APK из другого запуска или старой локальной сборки.
**Не удаляйте приложение с сохранениями только ради установки тестового APK.**

Для стабильной подписи добавьте в настройках репозитория **Settings → Secrets and variables → Actions** четыре repository secrets:

- `OLR_APK_KEYSTORE_BASE64` — base64-представление вашего Java keystore (целый файл без изменений).
- `OLR_APK_KEYSTORE_PASSWORD` — пароль хранилища.
- `OLR_APK_KEY_ALIAS` — алиас ключа.
- `OLR_APK_KEY_PASSWORD` — пароль ключа.

Никогда не коммитьте `.jks` и пароли в публичный репозиторий. Используйте существующий ключ подписи, если хотите обновлять уже установленную APK. Секреты не передаются сборкам от внешних pull request.

### Ограничение ANGLE/Vulkan

В проверенном дереве репозитория не оказалось `engine/android/app/src/main/jniLibs/arm64-v8a` с готовыми библиотеками ANGLE.
Обычная сборка не компилирует ANGLE из исходников. Даже если APK соберётся, **ANGLE/Vulkan может не работать** без этих библиотек.
Статус рендеров и функциональность следует проверять на настоящем устройстве.

## Тестирование изменений workflow

Открытый pull request с изменением самого workflow запускает пробную сборку.
Для кнопки ручного запуска через GitHub интерфейс файл с `workflow_dispatch` сначала должен оказаться в основной ветке `main`.


## Experimental original-R1-style multi-pass lighting (WIP)

The 2026-11-21 follow-up adds `-olr_r1_native` on top of both flags below.
It implements original lookup POINT/SPOT, lmap/vertex base materials and
direct UNORM additive color. See `OLR25_R1_REFERENCE_20261121.md` for the
corrected material identification and remaining renderer gaps. The older
forward and multipass experiments remain available for A/B tests.
This remains an incomplete R1 port; NDK/CI success is not a visual test.

This work-in-progress path is independent of the existing single-pass R1-forward prototype. Enable it **only** with both `-olr_r1_forward -olr_r1_multipass` on the OLR renderer. Omitting the second flag preserves the previous, tested R1-forward behavior.

- Base opaque world lightmapped geometry is drawn once from baked lightmap + ambient/hemi/sun.
- Each visible dynamic POINT or SPOT source with a light sphere intersecting the world visual is submitted as a separate additive pass, including the player's torch; static `bStatic` sources are excluded to prevent double-counting.
- Point-light cubemap faces are deduplicated by their shared parent.
- Each pass writes a tone-mapped **difference** into the post-combine LDR buffer, because the current engine is still carrying R2's post-processing. This is a transitional approximation, not a bit-identical copy of PC OLR's LDR blending.
- This prototype supports only Lua `default` opaque lightmapped materials. Projected spot cookies, light shadow occlusion, vertex-lit materials, alpha-test, original R1 geometry traversal, and removing the remaining R2 deferred stages are *not* yet implemented.

**Warning:** A large number of overlapping dynamic sources creates additional draw calls and may cost significant FPS. This is an isolated architectural experiment, not a performance release; keep the established APK installed separately until device testing passes. Verify dark basement (lamp on/torch off), torch cone, geometry, and FPS at 1728×1080. Build success alone does not establish GLSL runtime success.
