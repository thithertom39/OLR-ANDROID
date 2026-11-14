# ANGLE integration checkpoint — 2026-10-19

## New runtime checkpoint — real Vulkan context, failed image (2026-10-19 22:11:30 MSK)

This update supersedes older build-only/no-device/no-ANGLE-runtime wording below. The final `1.6.02-angle` APK is built and installed. Requested/effective `angle_vulkan`, live ANGLE/Vulkan1.3.128/Adreno740 identity and both APK `libGLESv2_angle.so` providers are verified. The same quicksave/l01_escape loaded and synchronization completed. **Visual acceptance failed: the world is black with contours. Global runtime_verified remains false; FPS/GPU time were not measured.**

Authoritative summary: `W/outputs/angle-work/angle-first-runtime-report.json`; portable copy and hashes: `R/evidence/angle-integration/`. Private raw logs/config/screenshot remain at `W/work/angle-runtime/angle-black-lighting-20261019-221130`; none are copied into the repository. Native baseline is preserved. Shader/FBO/fatal/OOM logs do not identify the fault, and Release lacks continuous error/FBO checks. Higher temperatures also preclude performance conclusions.

Exposure/tonemap feedback (including actual R32F filtering) and the existing highp-position sampler A/B are **hypotheses only**. Root is doing a bounded tonemap-off/restore-on experiment; its result is pending at this snapshot. Preserve raw first-capture evidence and write later experiments separately. Diagnose rendering correctness before lifecycle/performance acceptance; this work remains GLES through ANGLE, not a native Vulkan renderer.

## Snapshot boundary and scope

This document describes the inspected source after the Android startup fail-fast and ANGLE splash-skip follow-ups, and the already-built **angleprep** APK from before those follow-ups. It is a checkpoint during dependency acquisition, not a claim that ANGLE binaries or a functioning Vulkan context are available. The root agent will append the final build/export outcome separately.

- Windows workspace `W`: `C:/Users/User/Documents/Codex/2026-10-11/gjvyb`.
- Repository `R`: `W/work/openxray-build`.
- Base source commit: `28e6ff87203f0b93bf1935c9692b282d605b7072`.
- Existing uncommitted mapfix/archive-reader and ragdoll-audit changes were preserved; see `W/outputs/angle-work/baseline.json` and `mapfix-preexisting.patch`.
- Scope authorized by the user: prepare Linux dependencies, build pinned ANGLE, integrate **existing GLES through ANGLE/Vulkan**, retain Native GLES, and prepare a test APK. A native Vulkan renderer is **outside this work**.
- The user deferred tablet tests until after work. No device installation, game launch, force-stop, save manipulation, or overnight tablet action is authorized by this checkpoint. The root agent has not installed angleprep.
- No Git commit/push, release/source packaging, or publication was requested. Do not regenerate the larger release/handoff bundles. The root agent maintains current pointers in `PROJECT_STATUS.md` and `CONTINUE_PROMPT.md`.

## What is implemented and what is unverified

| Item | Actual state at this snapshot |
| --- | --- |
| Native GLES renderer | Preserved. Existing shadow, stream-upload and uniform-batching fixes remain. |
| Launcher route selection and status | Implemented and Java-compiled; distinguishes packaged availability from runtime verification. |
| Java environment/library transaction | Implemented and host-tested, including cleanup and restart requirements. Real Android library loading untested in this session. |
| Engine/ImGui GL dispatch | Unified through the renderer-owned GLAD table. Focused object/link tests and the full angleprep APK ELF audit pass. |
| Android startup failure handling | Source implemented and actual ARM64 syntax checked; newer than angleprep APK. Final APK rebuild still required. |
| ANGLE SDL 2D splash skip | Source implemented by root and inspected; newer than angleprep APK. Final build/device validation pending. |
| Linux host | Ubuntu-24.04 under WSL2, workspace inside Linux filesystem; dependency sync/sysdeps in progress according to root status. |
| Actual ANGLE `.so` files | Not built/exported/packaged at the snapshot. `jniLibs/arm64-v8a` was absent when checked. |
| ANGLE/Vulkan game rendering | Not verified. No live Vulkan renderer identity, scene comparison, FPS, GPU timings, or lifecycle result exists for this work. |
| Native Vulkan renderer | Not implemented or started. GLES API calls remain the engine's graphics interface. |

## Java route configuration

Production files, relative to `R`:

- `android/app/src/main/java/org/openxray/RenderBackendConfig.java` — new platform-light routing/transaction helper.
- `android/app/src/main/java/org/openxray/LauncherActivity.java` — `setupRenderBackendSpinner()`, `updateRenderBackendStatus()`, `launchGame()`, preference save/load.
- `android/app/src/main/java/org/openxray/OpenXRayActivity.java` — `loadLibraries()`, `configureRenderBackend()`, broken-SDL guards in Activity setup/resume.

The launcher retains three IDs:

- `native`: Native OpenGL ES, the default and recovery path.
- `angle_vulkan`: existing GLES renderer through ANGLE's Vulkan backend.
- `angle_gles`: existing GLES renderer through ANGLE's native GLES backend, useful as a translation/control route; its runtime is also unverified.

Preference key: `render_backend`. Intent extra: `extra_render_backend`. The selection does not replace `xrRender_GL` or add a `xrRenderVK` module.

### Availability is a package check, not proof of Vulkan

`RenderBackendConfig.probe(sdk, nativeLibraryDir)` requires **API 29 or newer** and a matching, nonempty, readable pair from the app's own `nativeLibraryDir`:

- `libEGL_angle.so`
- `libGLESv2_angle.so`

The API 29 gate follows this pinned ANGLE/Chromium configuration. Native GLES continues supporting the app's existing minSdk 24. No system/vendor ANGLE pair is searched or mixed with the APK pair. `android/app/build.gradle` retains `useLegacyPackaging = true`, so packaged native libraries are extracted into `nativeLibraryDir`.

An available pair means only that files are present. ABI, missing transitive dependencies, native linker namespaces and EGL/device creation can still fail. The launcher text explicitly says runtime verification occurs at startup rather than claiming Vulkan is active.

### Transaction before SDL/engine loading

`OpenXRayActivity.loadLibraries()` configures/preloads graphics before loading SDL, engine and renderer libraries. `RenderBackendConfig.ProcessSession.prepare()`:

1. Normalizes an absent/unknown route to `native`.
2. Clears `SDL_VIDEO_EGL_DRIVER`, `SDL_VIDEO_GL_DRIVER`, and `ANGLE_DEFAULT_PLATFORM` first.
3. For an available ANGLE route, sets absolute APK library paths as the two SDL drivers and sets `ANGLE_DEFAULT_PLATFORM` to `vulkan` or `gl`.
4. Preloads the matching pair via `System.load(absolutePath)`, EGL first, GLES second.
5. Returns both requested and effective selections. They are exported as `OPENXRAY_RENDER_BACKEND_REQUESTED` and `OPENXRAY_RENDER_BACKEND_EFFECTIVE` before SDL/engine loading.

On unavailable files, unsupported Android API, or a recoverable pre-context setenv/preload failure, all three driver variables are cleared and effective selection becomes `native`, with a reason in AppLog and a toast. Partially loaded native libraries are **not unloaded**; environment rollback and the process lock prevent a misleading route retry. Cleanup failure or failure to export the verification variables stops launch rather than continuing with ambiguous configuration.

An EGL/context/GLAD failure **after** successful preloading does not attempt to switch drivers silently inside the same process.

### Process lifetime and backend switching

`RenderBackendConfig.PROCESS` is a process-wide synchronized session. Native libraries and function-pointer/static state outlive an Activity; therefore changing to a different effective backend in that process is blocked.

- Same-route Activity recreation reuses the prepared selection without reloading/reconfiguring.
- Different route: save the preference, show explicit **force-stop OpenXRay in Android app settings and reopen** instructions, and stop the new launch.
- No automatic process kill or force-stop occurs.
- After ANGLE fell back to native, another ANGLE attempt requires a fresh process; explicitly selecting native can proceed.

`OpenXRayActivity.onCreate()` stops additional game/surface/touch setup if SDL sets `mBrokenLibraries`; resume is guarded as well. Do not remove these guards or interpret a Java preload success as successful EGL initialization.

## Native dispatch and startup verification

Production files:

- `Externals/imgui-proj/CMakeLists.txt`
- `src/Layers/xrRenderPC_GL/CMakeLists.txt`
- `src/Layers/xrRenderGL/glHW.cpp`
- `src/xrEngine/x_ray.cpp` — Android-only skip of optional SDL 2D splash when the effective route uses ANGLE.

### One GLAD table, no new loader

`xrRender_GL` already compiles `sdk/include/glad/gl.c` and owns its function-pointer definitions. `CHW::CreateDevice()` makes the SDL context current and calls `gladLoadGLES2(SDL_GL_GetProcAddress)` on Android.

Previously, ImGui's Android backend included system GLES headers and imported direct `gl*` symbols. Android-only source `COMPILE_OPTIONS` in `imgui-proj/CMakeLists.txt` now force the existing `sdk/include/glad/gl.h` **first for that backend translation unit only**. GLAD intentionally defines compatible GLES header guards and maps its calls to `glad_gl*` pointers. There is no second `gl.c`, no second load operation, and no independent ImGui dispatch table.

`xrImGui` remains a static archive. The OpenGL backend object is used by `src/Layers/xrRender/dxImGuiRender.cpp` in `xrRender_GL`; core ImGui archive use by other engine modules does not require another GLAD definition. Android direct `GLESv3`/`EGL` links were removed from both the archive and renderer. The renderer links `dl` for startup provider diagnostics. Desktop compilation/loading behavior is unchanged.

The vendored `Externals/imgui` submodule was briefly edited during investigation, then restored byte-for-byte to its original backup. Final implementation does **not** dirty that submodule. Original backend SHA256: `5eb66334ddd87ac06d590260f94dbe1b695aadef229bab5179083c90af7a5cfa`.

### Live context identity

Android-only `VerifyAndroidRenderBackend()` in `glHW.cpp` logs once per context initialization:

- requested and effective selections;
- SDL EGL/GLES library paths and ANGLE platform request;
- shared engine/ImGui dispatch description;
- `dladdr` providers for loaded `glGetString` and `glDrawElements` pointers;
- existing GL renderer/vendor/version/GLSL/capability logs remain.

Checks use **effective**, not requested, selection, permitting an explicit Java fallback to native:

- Effective `angle_vulkan` requires live `GL_RENDERER` containing both `ANGLE` and `Vulkan`.
- Effective `angle_gles` requires `ANGLE` and absence of `Vulkan`.
- Mismatch invokes non-ignorable `CHECK_OR_EXIT`, with recovery guidance. File presence, spinner state or environment alone is never accepted as proof.
- Effective `native` preserves ordinary system-driver semantics, including any firmware/system-level driver selection; it does not impose a Qualcomm-specific renderer string.

Provider names are diagnostic evidence, not an exhaustive symbol-ownership proof or a substitute for actual context identity and scene correctness.

### Fail-fast follow-up after angleprep

Three existing Android `CHW::CreateDevice()` failure branches now call `xrDebug::DoExit()` rather than returning from `void` and permitting later resource use:

1. No context after the unchanged GLES 3.2 → 3.1 → 3.0 attempts.
2. Failed `MakeContextCurrent(IRender::PrimaryContext)`.
3. GLAD initialization returns version zero.

Existing SDL error logs remain; fatal messages point to OpenGL/RenderBackend logs and Native GLES recovery. Non-Android GL and GLES keep their old return behavior. This is an initialization failure policy, not automatic driver migration or context-loss recovery.

Current inspected `glHW.cpp` SHA256 after the follow-up: `7369a9e8a0c3e85cb3fc3c3e051e573fff10de9dedc184713b68384caa802738`.

### SDL direct GLES imports still exist, for a different path

The angleprep `libxrRender_GL.so` has no undefined direct GL imports and no GLES/EGL `DT_NEEDED`. This must **not** be broadened into a claim that every library in the APK is free of system graphics dependencies.

The existing `libSDL2.so` still imports system GLES1/GLES2 entrypoints and needs `libGLESv1_CM.so`/`libGLESv2.so`, associated with SDL's compiled 2D GLES renderers. Relevant dependency sources are `W/work/deps/sdl2-src/src/render/opengles/SDL_render_gles.c` and `src/render/opengles2/SDL_render_gles2.c`. In Android `GLES2_LoadFunctions`, `__SDL_NOGETPROCADDR__` causes assignments from direct system symbols. The engine source audit found no direct `SDL_CreateRenderer`, `SDL_CreateWindowAndRenderer`, `SDL_RenderCopy`, or `SDL_RenderPresent` calls in `R/src`, but this does **not** exclude indirect SDL 2D use.

Specifically, `src/xrEngine/x_ray.cpp::ShowSplash()` / `SplashProc()` can call `SDL_GetWindowSurface()` and `SDL_UpdateWindowSurface()` when a splash image is available. `src/xrEngine/embedded_resources_management.h::ExtractSplashScreen()` loads `logo.bmp` outside Windows. SDL can implement that surface with `SDL_CreateWindowTexture()` -> `SDL_CreateRenderer()`, so the splash is a conditional path into the direct-system renderer while ANGLE driver selection is active. A related runtime failure has not been reproduced, but its source path exists; an absent logo is not a permanent guarantee against reaching it.

The root added a narrow Android-only guard in `CApplication` construction, before splash window creation. Exact effective routes `angle_vulkan`/`angle_gles` force `showSplash = false` and emit `SDL_Log("[RenderBackend] Skipping SDL 2D splash for %s", backend)`. Native/unset routes retain the existing `-nosplash` and `-splashnotop` behavior; Java fallback to effective native also retains normal native splash behavior. No SDL dependency source is changed. This source was inspected; the root's final whole build and device test remain pending at this checkpoint. `x_ray.cpp` SHA256: `5be00be6fb132e6dbdbd574a7d627cd074689248f66d63be8531e8201b04366d`. Original backup: `W/work/angle-integration-backup/native/src/xrEngine/x_ray.cpp`.

The normal engine context path uses SDL's EGL driver loading and `SDL_EGL_GetProcAddress` in `W/work/deps/sdl2-src/src/video/SDL_egl.c`, which honors the selected SDL driver variables. A remaining SDL system dependency does not by itself prove that the engine draw path bypasses GLAD. Equally, removing it without auditing callers is not a complete solution.

## Pinned Linux build recipe and dependencies

Canonical prepared recipe: `W/outputs/angle-build-linux.sh`.

- ANGLE: `e5640f4ec9ce5fec357186fd7fa40ad3030f2698`.
- depot_tools: `a03dfbf7f2d1cefa2795d81d59ca4f16c8616a8f`.
- WSL distribution: `Ubuntu-24.04`, WSL2; Linux storage on `D:/WSL/Ubuntu-24.04`.
- Linux workspace: `/home/user/oxr-angle-linux-e5640f4ec9ce`.
- Source: `<workspace>/client/angle`.
- Pinned tools: `<workspace>/depot_tools`, with `DEPOT_TOOLS_UPDATE=0`.
- Build output: `<source>/out/OpenXRayArm64`.
- Default export: `<workspace>/export-arm64/arm64-v8a` (must be empty/new before exporting).

Build inside the Linux filesystem, not `/mnt/c`. ANGLE obtains its own pinned Linux NDK/SDK, Chromium clang, GN and dependencies through DEPS. Do not point it at the Windows NDK used for the engine.

The `.gclient` excludes optional OpenCL, Dawn, Mesa, restricted and extra trace dependencies. Recipe sync command is `gclient sync --revision angle@<ANGLE_REV> --shallow --no-history --jobs <JOBS>`. `--sync-only` prepares the checkout and exits; full mode later runs GN/Ninja/export. Run one sync/build process at a time as Linux user `user`; only Linux package installation needed root privileges.

GN arguments in the inspected recipe:

```gn
target_os = "android"
target_cpu = "arm64"
arm_control_flow_integrity = "none"
is_component_build = false
is_debug = false
is_clang = true
angle_enable_vulkan = true
angle_enable_gl = true
angle_enable_null = false
angle_enable_trace = false
angle_enable_vulkan_validation_layers = false
use_remoteexec = false
use_siso = false
symbol_level = 1
```

Generation: `buildtools/linux64/gn gen out/OpenXRayArm64 --fail-on-unused-args`; compilation: `autoninja -C out/OpenXRayArm64 -j <JOBS> libEGL libGLESv2`.

Read-only inspection of the pinned checkout confirmed these GN options, DEPS provision of `buildtools/linux64/gn`, Android `_angle` suffix, `.gn` API level 29, and `ANGLE_DEFAULT_PLATFORM=gl` selecting OpenGL ES versus `vulkan` selecting Vulkan. The pinned Chromium `install-build-deps.sh` forwards to Python; `--android` remains functional but deprecated, and `--no-prompt`/`--no-chromeos-fonts` are supported. A partial dependency sync is not proof that GN/toolchains/hooks are fully ready.

### Export checks still required

The recipe exports the pair and recursively follows non-system `DT_NEEDED`, requiring each private dependency to be found and packaged. It records ELF metadata, revisions, hashes and license candidates. Important final checks:

- AArch64 ABI, correct SONAMEs, no unprovided private dependencies.
- Every `PT_LOAD` has compatible offset/address and **at least 16KB alignment**. The pinned Android build defaults to max-page-size 16384 for ARM64, but the prepared export script records metadata rather than asserting every alignment; final audit must assert it.
- Pinned release/noncomponent/nonsanitized builds normally link custom Chromium libc++ statically. Verify actual dependencies/symbols; do not replace the engine's existing `libc++_shared.so` with a different runtime.
- Match actual Android API requirements to the Java API gate and device. Keep native minSdk unchanged.
- Include real runtime dependency closure and required attribution. A collected license archive is evidence, not a completed public-release license review.
- Only after verification copy intended ARM64 runtime libraries into `R/android/app/src/main/jniLibs/arm64-v8a` and build a separately named test APK. Preserve working/rollback APKs.

## Evidence and exact validation limits

### Java

`W/work/angle-java-tests/README.md`, `run.py`, `results.json`, `android-javac.log`:

- 110 host assertions against the production helper: API gate, complete/partial/empty pair, both platforms, stale environment, each partial setenv/preload failure, cleanup failure, process recreation, restart blocking, concurrency.
- All three changed production Java sources compile against Android API 34 and cached AndroidX dependencies.
- Prior launcher regression suite passed 104 production-method assertions/contracts according to the Java test README.
- Runtime environment/library operations are injected mocks; no actual Android dynamic loading, UI, EGL or Vulkan is proved by these host tests.

### Native

`W/work/angle-native-dispatch-tests/README.md`, `validate.py`, `results.json`:

- Actual Android release compile configuration, actual ImGui backend source and the source-specific forced include.
- Original backend object: 54 direct `gl*` imports; configured object: zero direct imports and 54 GLAD references.
- Real ImGui/backend/one-GLAD link fixture passes `--no-undefined`, has a single GLAD table, and needs only libc/libm/libdl.
- Actual ARM64 Release syntax for `glHW.cpp` and desktop ImGui syntax pass.
- `verify_startup.py`/`verify-startup-host-results.json`: 15 cases using the exact production identity helper, with environment/provider/log/exit mocks.
- `verify_creation_failure.py`/`verify-creation-results.json`: exact production initialization prefix, 8 Android + 4 desktop GL + 8 desktop GLES cases; ordered version fallback and fatal/nonfatal resource-control flow checked.
- `glHW.angleprep.cpp` captures the native source before the fail-fast follow-up. Backups of original edited files are under `W/work/angle-integration-backup`; Activity originals are under its `java/` subdirectory.

### Actual preparation APK, historical artifact

`W/outputs/angle-work/angleprep-apk-report.json` is authoritative for this specific build:

- Artifact: `W/outputs/openxray-soc-angleprep-arm64.apk`.
- Version: `1.6.02-angleprep`; package `org.openxray.dev`.
- Size: 30,957,871 bytes.
- SHA256: `315fa65efa4c2a2016981042b001e46e29ed228cbc228a6d155b8764d8d983f2`.
- APK v2 signature verified; certificate matches mapfix; engine C++ runtime unchanged.
- 27 ARM64 ELF libraries, all reported 16KB-compatible.
- Final renderer has no direct GL imports or GLES/EGL dependency.
- Existing mapfix source preserved and source snapshot unchanged during that build.
- **ANGLE included: false; installed: false; runtime verified: false.**

The build input snapshot `W/outputs/angle-work/angleprep-inputs.json` records `glHW.cpp` SHA `b921639a...`, whereas current source is `7369a9e8...`. Thus angleprep does **not** contain the newer native fail-fast branches or the later `x_ray.cpp` splash guard. Preserve this historical distinction rather than editing its report to describe later source.

The existing Windows engine build remains `W/work/build.ps1` (process-local JDK/SDK/Gradle environment); the root performed the separate angleprep build. Do not rerun a full build concurrently with the root. No raw compile-command dump or private signing key is needed in a handoff.

Progress pointers: `W/outputs/angle-work/status.json`, `latest-sync.txt`, `latest-sysdeps.txt`, `latest-bootstrap.txt`. The inspected status at 20:51:49 MSK said dependency acquisition/sysdeps were in progress, binaries not built, runtime unverified. These are historical snapshot values; inspect later appendices/reports for final outcomes.

## Known constraints and preserved behavior

- ANGLE does not replace the gameplay engine, physics, task system or renderer pass algorithms. It does not establish a fix for the stretched corpse, archive-memory incident, quest completion or all device-specific rendering bugs.
- Keep Native GLES available and keep existing depth-only shadow detach, flashlight shadow writer, shared physical-state cache, stream upload and uniform batching. Do not re-enable rejected stream orphan/sync experiments or the slow state-rebind diagnostic as defaults.
- No automatic fullscreen-context recreation was added. `src/xrEngine/AndroidAppLifecycle.h::DrainRenderResetDiagnostics()` still logs `full_context_recovery_unimplemented` for genuine device/context loss. Preserved-context pause/resume had worked historically; ANGLE lifecycle itself is untested.
- No FPS gain, lower memory use, removal of foliage shimmer or new AA algorithm has been measured. FXAA quality options are the existing algorithm, independent of selecting Vulkan through ANGLE.
- Target devices remain Tab S9 first, S26 Ultra later. Driver capability, GLES extensions, precision, shaders, render-target formats and actual renderer identity need device evidence.

## Next validation and acceptance criteria

First complete pinned dependency acquisition/build/export and package a separately identified candidate containing the verified pair. Re-run final APK checks against the actual candidate: signatures, engine runtime, ABI, 16KB ELF alignment, private dependency closure, direct engine GL imports, source hashes, and explicit mitigation/verification of the conditional SDL splash renderer path described above. Do not mark ANGLE integrated operationally from successful compilation alone.

When the user is present and has provided the tablet for testing:

1. Back up existing app/config/saves and record the current installed APK before updating. Preserve game files and use the same package/signature.
2. Confirm Native GLES still starts and loads the intended save with unchanged options. Capture startup renderer/vendor/version, arguments, config and screenshot.
3. Force-stop/reopen explicitly between effective backend changes. Select ANGLE/Vulkan, load the same save, and require Java requested/effective logs plus native `[RenderBackend] verified live context: angle_vulkan`, actual ANGLE/Vulkan renderer identity and credible provider paths. Do not count a native fallback as a Vulkan run.
4. Check a loaded level, not only the menu: sun/foliage, flashlight shadows behind NPC/objects, depth, transparent surfaces/water, particles/fire, weapon/HUD, PDA/inventory/UI and postprocessing. Preserve logs around shader compilation/linking or FBO failures.
5. Test background/foreground, screen lock/unlock, surface changes and repeated loading. Distinguish retained-context resume from genuine context loss; current recovery limitation remains explicit.
6. Only after correctness, compare Native GLES, ANGLE/Vulkan and optionally ANGLE/GLES under identical saves, camera, resolution/render scale, effects, refresh/FPS policy, cache warm-up and temperature. Record frame-time distribution, CPU/GPU waits, memory and shader/pipeline cold/warm behavior. Existing `android_perf scene_ms` is CPU-side timing, not a GPU measurement.

Success requires real context identity plus stable, comparable gameplay rendering. Performance is an experiment outcome; no improvement is promised. A failure should leave a clearly identified Native GLES recovery path and preserved logs, rather than trigger speculative unrelated source changes.

## Final outcome appendix

### Linux build/export completed, 19 October 2026

The pinned ANGLE source compiled and linked both Android libraries successfully: Ninja completed all 1674 actions. GN generated 2540 targets from 274 files; `--fail-on-unused-args` passed. The accepted full sync/GN/Ninja/export job is `W/outputs/angle-work/build-20261019-213606/result.json`, exit 0 at 21:37:47 MSK. Earlier job failures/cancellations remain historical evidence and are not overwritten as successes.

Completed export: `/home/user/oxr-angle-linux-e5640f4ec9ce/export-arm64-final/`, including the pair, ELF metadata, `SHA256SUMS.txt`, `ANGLE-DEPS.txt`, actual Git/GCS revisions, local installed CIPD instance IDs, GN arguments, ANGLE license and `source-license-notices.tar.gz`. The older `export-arm64` and `export-arm64-final.partial-20261019-213239` are incomplete diagnostic exports; do not package them.

Actual raw libraries:

| Library | Bytes | SHA256 | DT_NEEDED |
| --- | ---: | --- | --- |
| `libEGL_angle.so` | 311128 | `89be434d2e3340ea216d1a681641a4041d1aef827f32360e76035f4beea9e182` | `libdl.so`, `libc.so` |
| `libGLESv2_angle.so` | 6583208 | `fa8cd4351f84a0dfb9cbaeb81cbe91260492e9ff54779ac3917441380b576c9b` | `libdl.so`, `libm.so`, `libnativewindow.so`, `liblog.so`, `libc.so` |

`W/work/angle-native-dispatch-tests/verify_angle_export.py` passed against this completed export. Report: `W/outputs/angle-work/angle-export-report.json`, `export_verified=true`, errors empty. The independent checks include manifest hashes, AArch64, exact SONAMEs, Build IDs, PT_LOAD alignment/congruence >=16KB, dependency closure and strong undefined symbol resolution/version matching against the actual pinned Android API29 stubs. Weak imports and dynamic `dlsym`/driver/runtime limits are reported separately. Actual GN/link lines show API29 and static Chromium libc++/libc++abi archives; no new shared C++ runtime is needed. This proves binary/package compatibility conditions, not an Android linker or GPU execution result.

The verified pair was copied without replacing different existing files into `R/android/app/src/main/jniLibs/arm64-v8a/`; raw hashes match the export. `W/work/freeze_angle_inputs.py` froze seven edited/new production sources plus both libraries into `W/outputs/angle-work/angle-inputs.json`, and verified the five protected mapfix/questlife source hashes. It refuses to overwrite an existing freeze. `W/work/build.ps1 -Variant angle` is now the separate final APK build; its result is recorded below when complete. No device actions were performed.

### Build-recipe corrections and first-setup recovery

The canonical `W/outputs/angle-build-linux.sh` now includes three corrections discovered from real execution:

1. Call pinned `depot_tools/ensure_bootstrap` when its hermetic Python marker is absent. `gclient` used vpython successfully but the `autoninja` wrapper otherwise stopped with `python3_bin_reldir.txt not found`. This supported bootstrap does not update the pinned depot_tools Git revision.
2. Recognize `libnativewindow.so` as an Android public system dependency. The compiler/linker succeeded, but the initial exporter misclassified it as a private library. The actual API29 stub was checked; no Android system library was bundled into the APK.
3. Use `gclient revinfo --actual --ignore-dep-type=cipd` plus `.cipd_client installed -root ... -json-output ...`. Ordinary `revinfo --actual` still tried remote CIPD tag descriptions and stalled during metadata collection. The replacement records actual locally installed immutable CIPD instances without that redundant network lookup.

`W/work/angle-linux-build.sh` uses `JOBS=4` and `ANGLE_EXPORT_DIR=<workspace>/export-arm64-final`. The canonical recipe still requires an empty/new export directory, so do not blindly rerun it against the completed export. For another export use a new explicit `ANGLE_EXPORT_DIR` or a new wrapper, preserving existing accepted artifacts.

WSL had intermittent IPv4 TCP connection failures to Google; bounded Windows requests to the same official services were more reliable. No DNS, hosts, proxy, firewall or global Windows settings were changed. Diagnostics: `W/work/angle-network-review/`. Recovery used separate Windows staging and exact upstream content/revisions:

- CPU features commit `81d13c49649f0714dd41fb56bb246398b6584085` was fetched into a separate bare Windows cache, fsck-checked, then locally imported into the otherwise unborn Linux dependency with `--update-shallow`; origin and exact commit stayed unchanged. Scripts/report: `W/work/angle-import-staged-cpu.sh`, `W/work/angle-network-cache/cpu_features-stage-result.json`.
- The i386/amd64 sysroot archives matched source-pinned SHA256. `W/work/angle-import-staged-sysroots.py` used the original pinned installer in memory with a local-file transport, preserving its checksum validation, extraction and stamp behavior. Original source was not edited. Report: `W/outputs/angle-work/local-sysroot-import.json`.
- Windows fallback copies of clang-format and Rust were prepared and checked, but their active Linux download eventually succeeded independently. Do not describe those fallback archives as imported. Rust's computed SHA256 is recorded, and MD5 matched official HTTPS object metadata; there was no source-pinned trusted tar SHA256. The bootstrap stage0 checksum applies to a different file.
- The two deliberate job-group cancellations affected only verified isolated WSL dependency/metadata jobs; compilation outputs were retained. Job logs keep their nonzero/cancelled result. No apt/package transaction was interrupted.

Linux package installation finished successfully at 21:07:29 MSK; full source sync/hooks and compile later succeeded. Git 2.43 emitted a recommendation for >=2.46, but this was a warning and did not prevent the successful build; no Git upgrade was performed.

### Final APK accepted offline, 21:40 MSK

`W/work/build.ps1 -Variant angle` completed successfully in 2m19s (47 tasks, 23 executed, 24 up-to-date). Final log copy: `W/outputs/angle-work/angle-apk-build.log`. Release compilation, CMake/native linking, Java/resources/dex, lintVital and packaging passed. This is not a claim that the previously failing full `lintRelease` is clean; that broader preexisting lint result remains in the older checkpoint.

- APK: `W/outputs/openxray-soc-angle-arm64.apk`.
- Package/version: `org.openxray.dev`, `1.6.02-angle`.
- Bytes: `33692484`.
- SHA256: `7557b74aaeeab740201b0cebce2500c3a89a45be11d28676aa084564bc7fb184`.
- Signature v2 verified; certificate SHA256 `606162651241a780913dcff90a4bdd997899cb5aad606c44273d2d0ad3d4e244`, matching mapfix.
- 29 AArch64 ELF libraries, including both ANGLE libraries; all load segments 16KB-compatible, dependency closure valid, exact ANGLE SONAMEs checked.
- Every engine module has zero undefined direct `gl*`/`egl*` imports; renderer has no direct system GLES/EGL dependency. SDL's 76 internal 2D GL imports are explicitly recorded, and the ANGLE-only splash guard is present in final `libxrEngine.so`.
- Engine `libc++_shared.so` exactly matches the mapfix APK. BuildConfig GitHub token is empty. Frozen sources/raw JNI libraries, build wrapper/recipe, and all five protected previous source hashes matched after building. ZIP CRC and APK manifest extraction mode passed.
- **Installed=false, runtime_verified=false, device tests deferred by user.** No current Vulkan context, shader compilation on Adreno, image, save/load, resume or FPS measurement exists. The tablet's last confirmed installed build stays mapfix. Both angleprep and final angle were never installed in this session.

Authoritative final report: `W/outputs/angle-work/angle-apk-report.json`, generated by `W/work/verify_angle_apk.py --variant angle --expect-angle present`. The checker refuses to overwrite a different already-exported APK. For future source changes choose a new variant/input snapshot rather than overwriting the accepted freeze/report/artifact.

A portable copy of the tested Linux recipe is now at `R/scripts/build_angle_pinned_linux.sh`; only its documentation header was updated to reflect the completed real build. Commands and pins match the accepted recipe in W. Prefer this pinned recipe over legacy `scripts/build_angle.sh`, whose main-branch/system-SDK assumptions were not validated in this work. The recipe does not install Linux packages, accept licenses or publish anything. Linux build dependencies were installed separately using the pinned Chromium `build/install-build-deps.sh --android --no-prompt --no-chromeos-fonts`.

Important for future Git/source transfer: `.gitignore:63` ignores `arm64-v8a/`, so the new raw JNI libraries are present locally but will not appear in an ordinary Git clone. Rebuild/export and copy them, or preserve the verified binaries separately with their hashes; do not assume that copying tracked code includes them. No Git commit/push or public release/source archive was created.

The next authorized device step is **after the user returns**: backup the installed mapfix/config/saves, install this exact candidate without deleting data, verify Native GLES first, then force-stop/reopen between backend changes and test ANGLE/Vulkan with the same loaded save. Require live renderer verification and correct gameplay before comparing performance. No extra launch flag enables ANGLE: use the launcher render-backend selector; retain the existing Normal profile/options for a controlled comparison.

Portable report copies are under `R/evidence/angle-integration/` (README describes each); full working logs/helpers remain under W. `W/outputs/project-state.json` retains installed mapfix fields and adds a separate uninstalled `angle_candidate`, so the candidate is never confused with device state. `W/outputs/angle-work/status.json` now says `APK_READY_DEVICE_PENDING`, with no active build. The unused lighttpd service automatically installed by Chromium's Linux dependency script was stopped and disabled inside the new Ubuntu environment; no global Windows settings were changed.

### Subsequent authorized device installation, 21:56 MSK

The user returned, connected the tablet and resumed device testing. `W/work/install_angle.py` updated actual SM-X710/R52X40ES4LE from mapfix to angle using `install -r` only after inactive-game checks, backup, exact candidate/previous-APK hashes and matching certificates. Backup: `W/work/angle-runtime/install-20261019-215625`. Report: `W/outputs/angle-work/angle-installation-report.json`. UID10361 remained unchanged; the pulled installed APK exactly matches the accepted candidate hash. `user.ltx`, all four savedgame files and two optional external configs are byte-identical before/after. No uninstall/clear-data was used. Private launcher/touch preferences were not extracted; release is nondebuggable and no root/run-as backup was assumed.

Launcher opened; UI XML is at `W/work/angle-runtime/launcher-ready.xml`. Current selections: Native GLES, 1728×1080, Extreme, FXAA on/Fast, Normal, FPS unlock off. A previous-crash dialog cited 15 October and was skipped; this does not indicate an angle startup crash. Gameplay has not yet started. Next step: user loads the same save on Native GLES, capture actual new renderer/logs, then restart the app process and select ANGLE/Vulkan for the same scene. Device runtime/ANGLE/Vulkan/gameplay/FPS remain unverified. The earlier offline build reports intentionally keep installed=false; this installation supplement and current project/status JSON take precedence for installed state.


## 2026-10-20: angleaudit installed; waiting for diagnostic save load

- Valid test `-gles_highp_position` (capture `work/angle-runtime/angle-highp-position-corrected-20261020-135814`, PID19762) verified `highp (diagnostic)`, actual ANGLE Vulkan, loaded quicksave; scene still near-black. Precision change ALONE did not solve blackout. Earlier typo attempt remains invalid. Summary `outputs/angle-work/angle-highp-position-result.json`.
- New diagnostic-only APK `outputs/openxray-soc-angleaudit-arm64.apk`, 1.6.02-angleaudit, SHA256 fe83fc6aea044d2e645e97784e45dd39f4c5005e0ed4f0f32e79f000ddbc0dd5, 33694876 bytes. Gradle assembleRelease passed in56s; signature/29ARM64ELF/16K/sharedCRT/dispatch/frozeninputs/protectedmapfix files passed. Accepted original angle APK remains unchanged.
- User exited game normally; install-r completed 2026-10-20 14:27MSK, UID10361 retained, installed APK exacthash verified, user.ltx+4savedgame files+2externalconfigs unchanged. Backup `work/angle-runtime/install-angleaudit-20261020-142658`. Private launcher/touch prefs preserved by update, no root/private backup claimed. Separate report `outputs/angle-work/angleaudit-installation-report.json`; original angle install/build reports are immutable historical evidence.
- Code changes ONLY opt-in `-gles_angle_audit`: glHW.cpp exact-token flag, synchronous debug callback with independent64critical/16warning caps (warning flood cannot hide real errors), float-linear/color-buffer extensions and fragmentprecision logs; r2_rendertarget_phase_luminance.cpp reads owned complete1x1R32F target before/after final draw on max8frames, using actual implementation-reported RED/FLOAT orRGBA/FLOAT, restoring per-FBOREAD_BUFFER/PBO/pack state and original READ_FBO. No draw-FBO/attachment/cache mutations, no rendering-algorithm/shader changes. Readback consumesGLerrors and canstall: DIAGNOSTIC, never use for FPS comparison. Default normal path has no added diagnostic GLcalls.
- Focused actual-helper tests:22flag/6filter/7length cases, independentquota+concurrency, realAndroidNDKsyntax; luminance17cases include differentREAD/DRAW, boundPBO/packoffsets, REDread, error/unsupported paths, defaultoff0GLcalls, plusreal full.cppNDKsyntax. Reports in `work/angle-audit-tests`, portable JSON summaries in `evidence/angle-integration`; hostfixtures do not establish actualdriver correctness.
- Current task: user opens launcher, retains ANGLE Vulkan + `-gles_highp_position`, adds ONLY `-gles_angle_audit`, MAIN MENU then same quicksave, renders unpaused10-15s, root captures via `work/capture_angle_runtime.py`. Verify actualdiagnosticstartup+gate+restored=1 before interpretingvalues. No diagnostic game run yet; blackworld rootcause remains UNKNOWN.
- Hypothesis to assess from actualcaps/readback: R32F exposure feedback uses LINEAR and mayrequire unavailableOES_texture_float_linear. Not proven. Pinned ANGLE returns GL_RED/FLOAT forR32Factualformat (unlessfallbackformatdiffers), so probe supportsRED. Iffloatlinearabsent andexposurebad, consider nearestfilter on1x1feedback as minimalfix AFTER evidence; do notblindlyaltertonecurve/Gbuffers. Other candidates: sampledtexturecompleteness, FBOerrors, Gbuffer inputs. Existing negative tonemapoff test stilldoesnotexclude feedbackfailure.
- Build: `./work/build.ps1 -Variant angleaudit`; inputfreeze outputs/angle-work/angleaudit-inputs.json. Verifier `work/verify_angleaudit_apk.py --variant angleaudit --expect-angle present` (copyoforiginalwithdiagnosticpayloadchecks/currentdeviceauthorization); installer `work/install_angleaudit.py` guardsactivegame, backups, signature/hash/data checks. First preflight attempt used oldreport path and correctly refused reinstall before ANYdevice mutation; helperpath corrected, later install verified. Do not rerun installer when sameAPKalreadyinstalled.
- No Git commit/push/releasepackaging, no Windows globalsettingchanges, no WSL/ANGLErebuild; existingNativeGLES route preserved. NativeVulkan remainsoutofscope.

## Latest accepted result — 20 October 2026, 16:15 MSK

The earlier black-world failure, pending angleaudit load, pre-installation and paused-work statements above are historical. **The tested exposure defect is fixed.** Continue from [ANGLE_EXPOSURE_FIX_2026-10-20.md](ANGLE_EXPOSURE_FIX_2026-10-20.md), which records the causal single-sampler experiment, final source changes, tests and remaining limits.

The actual ANGLE/Vulkan context lacks `OES_texture_float_linear`; LINEAR sampling of the 1×1 R32F previous-exposure history was incomplete despite framebuffer completeness and float renderability. The isolated GLES POINT sampler change restored the same save's visible world/sky/NPCs and changed late exposure readings from approximately 0.10 to 1.514. Final `1.6.02-angleexposure2` also gives the nine Lua cloud/volumetric/portal tonemap consumers explicit POINT/CLAMP/noMIP contracts. Highp-position alone did not solve the defect; final Normal tests require neither highp nor audit flags.

Installed APK `W/outputs/openxray-soc-angleexposure2-arm64.apk` has SHA256 `d31ef8e016b42f4d65efd81334f477fc6c098c018d210f9b3de58ae5b7826abe`. The user authorized root's remote device operation. The same SoC quicksave/l01_escape passed limited stationary scene checks on Native GLES and actual ANGLE/Vulkan with identical `user.ltx`, empty additional args and default position sampler. A screen sleep/wake returned a visible scene in the same Vulkan process. Root inspected the captured images. No FPS/GPU/CPU performance comparison, full gameplay, full context-loss recreation, S26 Ultra or CS/CoP runtime acceptance is claimed.

Current summary and portable evidence: [angleexposure2-runtime-report.json](../../evidence/angle-integration/angleexposure2-runtime-report.json). The separate build/input/assets/installation/cleanup summaries are beside it; immutable earlier reports retain their original build-stage flags. Raw screenshots/configs/logs remain private under W/work. Final cleanup closed the game normally, verified onDestroy and four unchanged savedgame files, retained Normal/empty extra args and angle_vulkan selection, and put the screen to sleep. Next is the user's moving/light/shadow/interaction review, followed by controlled Native-versus-ANGLE measurements. Native GLES remains available; native Vulkan and Git/release publication remain outside this completed scope.

