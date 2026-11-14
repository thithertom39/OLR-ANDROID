# OpenXRay Android source handoff — 2026-11-08

## Authority and latest APK
This package combines the CURRENT working tree (including all unpushed files) with the nine reviewed source files actually compiled into the latest APK, 1.6.02-skadovsk3 / org.openxray.dev.
APK SHA256: 434e72bd687eb1c2d738ac4e29cca271d19510a53db6f62001e5892f5f537229.
Original repository HEAD: 28e6ff87203f0b93bf1935c9692b282d605b7072; branch: main.
The original working tree is NOT modified or reset. Its complete uncommitted status is HANDOFF/ORIGINAL_GIT_STATUS.txt; tracked changes are also preserved as a binary Git patch. The package is independently committed locally; its snapshot commit is recorded in HANDOFF/SNAPSHOT_COMMIT.txt. Git history is not included in ZIP; all populated submodule sources are included as ordinary editable files.

HANDOFF/LATEST_APK_SOURCE_RECONCILIATION.json identifies the nine files recovered from the frozen last-APK stage. Unknown overlapping changes cause packaging to fail instead of overwriting them. Other current files are retained. This is a source handoff, not a new implementation or a rebuilt APK. Historical PROJECT_STATUS/CONTINUE_PROMPT documents are older; THIS FILE takes precedence.

## Build tools / dependencies
Windows 11 / PowerShell; JDK 17 (tested 17.0.20.1+1), Gradle wrapper 8.2, Android Gradle plugin 8.2.2, SDK platform 34, build-tools 34.0.0, NDK 27.0.12077973, CMake 3.22.1; minSdk24/targetSdk34; arm64-v8a.
Install with your Android SDK command-line tools:
```
sdkmanager "platform-tools" "platforms;android-34" "build-tools;34.0.0" "ndk;27.0.12077973" "cmake;3.22.1"
sdkmanager --licenses
```
Run from this source root (JAVA_HOME must point to JDK17):
```
powershell -ExecutionPolicy Bypass -File HANDOFF/build-apk.ps1 -SdkRoot C:/Android/Sdk -Keystore C:/path/to/your-own.keystore
```
Use your own signing key; original keys, passwords and private credentials are excluded. A different certificate cannot update the installed test APK without uninstalling it; do not uninstall it or remove user data as part of diagnostic development. APK output: android/app/build/outputs/apk/release/app-release.apk.

Vendored dependency sources: {"lzo-src": "No Git metadata (source tarball)", "ogg-src": "e1774cd77f471443541596e09078e78fdc342e4f", "openal-soft-src": "d3875f333fb6abe2f39d82caca329414871ae53b", "sdl2-src": "8236e01a9f758d15927624925c6043f84d8a261f", "vorbis-src": "0657aee69dec8508a0011f47f3b69d7538e9d262"}. Exact revisions recorded in HANDOFF/DEPENDENCY_REVISIONS.json. The separate Gradle init script supplies FETCHCONTENT_SOURCE_DIR_* paths; production CMake code is unchanged. No vendor-cache.cmake file is needed. Default direct build without init script fetches ogg v1.3.5, vorbis v1.3.7, LZO2.10, OpenAL1.23.1 and SDL release-2.30.12 as declared in cmake/XRay.Android.cmake. Gradle/Maven artifacts download normally. Original submodule URLs and commit gitlinks are preserved in .gitmodules and HANDOFF/SUBMODULE_*; no submodule download is required for included populated directories. Do not blindly run submodule update on top of customized bundled files.

Compiled ANGLE runtime libraries are included in android/app/src/main/jniLibs/arm64-v8a; they are dependencies, not private build outputs. Full ANGLE source tree/toolchain is NOT included. ANGLE is not rebuilt by normal APK builds. Renderer profiling requires no ANGLE changes. See existing Android/ANGLE build documentation for historical provenance; do not claim an unverified ANGLE source revision.

Release uses standard allocator, C++17 RTTI/exceptions, NDK Release optimization and thin LTO linker flags; see android/app/build.gradle and cmake/. GPU profiler defaults OFF. Presence of OXRAY_GPU_PROFILE property enables it, so omit the property for OFF (passing false still counts as present). For a dedicated diagnostic binary add -GpuProfile to the wrapper and runtime -gles_gpu_profile. Original staging-only absolute dependency paths/FORCE settings are deliberately NOT transplanted; all engine algorithm source matches the last APK. HANDOFF/LAST_APK_INPUTS.json records original compilation inputs/hashes; local toolchain paths in it are historical evidence, not prerequisites.

## Diagnostic-only assignment for receiving developer
Investigate CoP near Skadovsk; user reports 25–35 FPS and scene rising roughly9ms to25–31ms in gles_perf. Do NOT implement new rendering optimizations yet. Existing measured culling change and ragdoll5 must be preserved.
- src/xrEngine/device.cpp: gles_perf (~5sec eligible active gameplay) and bounded frame_trace (40 verbose frames; watchdog continues). Normal profile strips gles_perf; choose Launcher Diagnostic(logs) for periodic output. frame_trace quietness does NOT establish halted logging.
- src/Layers/xrRenderGL/glGpuProfiler.cpp/.h: existing async GPU timestamp instrumentation, compile flag XRAY_GPU_PROFILE_DIAGNOSTICS. CPU wall time is not GPU busy time; nested scopes must not be summed.
- android/app/src/main/java/org/openxray/AppLog.java and LauncherActivity.java: log capture/export. Confirmed export issue: findEngineLog searches mode root and case-sensitive OpenXRay prefix, while fsgame routes native logs to mode/_appdata_/logs/openxray_*.log. Fix discovery/export without changing render behavior.
- src/xrCore/log.cpp: Android AddOne logs to logcat and flushes native file each line; force_flushlog is redundant. App log is lifecycle events, not per-frame telemetry.
- Latest invalid-only spawn diagnostics are included. Oct31 02:03 living NPC activation-position failure remains unconfirmed cause; do not clamp coordinates or remove assertions.
- Font batch length correction and conservative local-light detail shadow culling are in the APK source; cull defaults ON, exact -gles_no_detail_light_cull opts out. Ragdoll5 AMotor fix is preserved.

Comparison evidence under HANDOFF is historical diagnostic data, not a guarantee about current user FPS. Native OFF16.64 versus ON36.77/37.22; ANGLE OFF33.14 versus ON35.88 at1728x1080, fixed copied save. Natural frequencies differed. Full final normal-APK hardware acceptance and SoC smoke were NOT completed. No fresh benchmark was run for this handoff.

## Exclusions and validation
No original commercial game archives, levels, saves, user.ltx from devices, APKs, signing keys, caches, .git metadata, logs or generated build outputs are included. Repository-owned shaders/config/script compatibility overlays and launcher UI resources ARE included because they are source/build inputs; these are not the user's installed game data. A locally signed APK still requires the user's legitimately obtained game files separately.
HANDOFF/FILES_SHA256.json records every packaged file except itself and snapshot metadata. ZIP is checked for CRC errors and fully extracted to a separate validation directory; manifest hashes and required source paths are rechecked. No performance tests or new APK build were performed.
