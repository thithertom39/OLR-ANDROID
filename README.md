# OLR 2.5 Android

Рабочая копия движка находится в каталоге `engine`. Исходный снимок взят из `C:\Users\User\Documents\ChatGPT\stalker\OpenXRay-Android` 12 ноября 2026 года. В исходном каталоге нет `.git`. Исторические сведения о Git лежат в `engine/HANDOFF`. Скопированы файлы рабочего дерева, включая локальные правки GLES-гаммы, погоды, теней и сохранений. Каталоги предыдущей сборки пересоздаются.

Пакет приложения — `org.openxray.olr`, ABI — ARM64. Игровая установка на ПК находится в `R:\project\STALKER-OLR2.5`. Проверенная версия ресурсов — OLR 2.5 + FIXX 59. APK содержит Android-слой и GLSL-файлы движка. Для запуска требуются 18 архивов из этой установки, всего 5 200 360 773 байта.

## Сборка и установка

Использованы JDK 17, Gradle 8.2, Android SDK 34, Build Tools 34.0.0, NDK 27.0.12077973, CMake 3.22.1 и MinGW для генератора LuaJIT. Сборка берёт исходники нативных библиотек из `engine/vendor`. Параметр `-Offline` использует уже заполненный кэш Gradle.

```powershell
$toolchain = 'C:\Users\User\Documents\Codex\2026-10-11\gjvyb\work'
$adb = "$toolchain\android-sdk\platform-tools\adb.exe"
$serial = 'R52X40ES4LE'

.\build.ps1 -BuildName 'OLR-Android-local-arm64' -Offline `
    -SdkRoot "$toolchain\android-sdk" `
    -JavaHome "$toolchain\toolchain\jdk-17.0.20.1+1" `
    -GradleCache "$toolchain\gradle-cache"

& $adb -s $serial install -r .\artifacts\OLR-Android-local-arm64.apk
.\stage-data.ps1 -Adb $adb -Serial $serial
.\test-device.ps1 -Adb $adb -Serial $serial -RunName 'local-menu' -SkipIntro
```

Лаунчер создаёт принадлежащий приложению каталог `/sdcard/Android/data/org.openxray.olr/files/olr`. `stage-data.ps1` переносит архивы по одному и сверяет размеры. При обновлении APK через `adb install -r` повторная передача архивов не требуется. `test-device.ps1` запускает лаунчер, проверяет состояние `18/18` и собирает снимок экрана, logcat, журнал приложения и нативный журнал OpenXRay в `logs`. Для прямого запуска новой игры есть `-Action NewGame`.