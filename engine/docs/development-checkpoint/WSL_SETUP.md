# Linux для ANGLE: рекомендуемая схема WSL2

Проверено по официальным инструкциям15.10.2026. Это инструкция; WSL/Ubuntu/BIOS агентом не устанавливались и не изменялись.

Рекомендуется WSL2 + Ubuntu24.04 LTS. Дуалбут и перенос Codex/основного OpenXRay-проекта не нужны. Linux будет собирать ARM64 библиотеки ANGLE; .so затем копируются в Android-проект, который по-прежнему собирается в Windows. ANGLE пока отсутствует в APK. Сам Vulkan backend после сборки библиотек потребует интеграции и проверки на планшете, не включится автоматически от установки Ubuntu.

На момент read-only проверки: Ryzen7 5700X, ~24GBRAM; firmware virtualization выключена, HypervisorPresent=false, WSL не установлен. На C: ~57GB свободно, на D: ~525GB. Рекомендуемая папка Linux-диска D:\WSL\Ubuntu-24.04; это отдельная папка, разметка дисков и загрузчик не меняются. Для зависимостей/сборки ANGLE нужен запас десятков GB; точный размер ещё не измерен.

Выполнять после сохранения текущей работы и когда можно перезагружаться:

1. В BIOS/UEFI включить SVM Mode / AMD Virtualization. Точное положение пункта зависит от материнской платы; другие параметры не менять.
2. PowerShell от администратора:

```powershell
wsl --install --no-distribution
```

3. Перезагрузить Windows. Затем PowerShell от администратора:

```powershell
wsl --install -d Ubuntu-24.04 --location D:\WSL\Ubuntu-24.04
```

4. При первом запуске Ubuntu создать пользователя/пароль Linux, затем проверить в PowerShell:

```powershell
wsl -l -v
```

У Ubuntu нужна VERSION2. Современный WSL поддерживает --location; если конкретная установленная версия отвергнет параметр, сохранить текст ошибки и сначала проверить wsl --version/--help, не переходить к переразметке или переустановке Windows.

После готовности среды агент создаёт ANGLE checkout и build внутри Linux filesystem (например ~/oxr-angle), а не на /mnt/c или /mnt/d: это снижает накладные расходы файловой сборки. Виртуальный Linux-диск при этом физически лежит в выбранной папке D:. Основной OpenXRay checkout остаётся Windows; между средами передаются готовые .so и отчёты. Существующие Windows SDK/NDK не подставляются в Linux ANGLE toolchain. Начать с сохранённого outputs/angle-build-linux.sh и ANGLE-NEXT-STEPS.md, сверив pinned DEPS/пакеты и запустив Linux deps step. Эти ANGLE-рецепты ещё не исполнялись.

Codex Windows может вызывать wsl CLI, поэтому отдельный Codex внутри Ubuntu не требуется. Текущий агент/диалог остаются Windows-native. Тесты графики выполняются на Samsung, WSL GPU passthrough для кросс-компиляции Android библиотек не нужен.

Официальные источники:

- https://learn.microsoft.com/en-us/windows/wsl/basic-commands
- https://learn.microsoft.com/en-us/windows/wsl/install
- https://learn.microsoft.com/en-us/windows/wsl/filesystems
- https://learn.chatgpt.com/docs/windows/windows-app#_windows-subsystem-for-linux-wsl
- https://chromium.googlesource.com/angle/angle/+/HEAD/doc/DevSetupAndroid.md
