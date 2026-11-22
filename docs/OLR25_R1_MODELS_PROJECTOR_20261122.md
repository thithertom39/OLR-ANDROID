# Native R1 model/projector checkpoint, 22 November 2026

Repository: R:/project/OLR-R1-parity, branch feature/olr-r1-gles-parity-20261121.
Draft PR: https://github.com/thithertom39/OLR-ANDROID/pull/8.
Full independent R1 goal remains active and incomplete. This is a component checkpoint.

## Implemented

- Original OLR `model_def_hq`/`model_def_lq` lighting, projected RGB + sun mask,
  source LQ hemisphere/ambient/sun, factor, world-position clamp, diffuse x2,
  authored alpha reference/blending and original fog. No R2 tone mapping.
- SKIN_NONE/0/1/2 use the existing GLES converted vertex adapter. In particular,
  original LQ SKIN_2 uses the first bone; HQ/local shaders retain weighted skinning.
  Existing GLES 3/4-weight formats also compile, but full scene parity is unproven.
- Lighting-only `lmap_l`, `vert_l`, `impl_l` equivalents, separated from R2 E[4].
  Projector surfaces have no diffuse RGB/detail/sun-color/fog contribution.
- Original projector: RGBA8 512x512, 51x51 tiles / 100 receivers, top-down view,
  source bbox, clamp, UVgen/half-texel/factor, bounds and 30–90 second cache.
  This is an R1 lighting atlas, not an R2 shadow map.
- Explicit atlas-only D3D-to-GLES raster/depth adapter. Original world rendering
  and existing hybrid/R2 behavior are preserved.
- Actual sector/owner information is propagated from the common visibility query.
  Android cross-library RTTI returned null for these real spatial objects and
  incorrectly forced all models to LQ. ROS uses the known owner to exclude self
  from sun rays, retaining the original ray tests and formulas.
- Newly created ROS is initialized before seeding a long-lived projector tile.
  This prevents caching an uninitialized zero average. It does not raise gamma,
  ambient, sun or other authored coefficients.
- `-olr_r1_projector_capture` saves one real atlas and one isolated model image.
  Diagnostic model draws are not presented as a complete world; coverage guard
  still returns the incomplete independent mode to the menu.

## Source and commit trail

The original OLR HLSL files under R:/project/unpack/all/shaders/r1 are the
formula reference. C++ projector and ROS reference is OpenXRay/xray d7b2359,
not a claim that the public source equals OLR's modified PC DLL.

- `6c843936f538628c11c4089b50efef044b3eb036`: paused model/projector WIP.
- `c75049d`: Adreno rejected the declaration named `clip`; renaming it fixes
  all three actual projector VS compilations. Mesa had accepted the old code.
- `c84558d`: known spatial owners, projector/HQ eligibility and ROS self owner.
- `7280d31931083c507f158abdf0b4e6beb1295a90`: ROS initialized before atlas cache.

Changed groups: Shader metadata/equality, ResourceManager and GLES B_MODEL
compiler, common graph build/render hooks, independent pipeline, olrR1Models,
olrR1ObjectLighting, GL CMake list, model/projector GLSL, GLES probe and capture
inspection tool. No original project, launcher UI or third-party game assets
were changed or added to the public repository.

## Confirmed validation

Logs and raw captures: logs/20261122/models-projector (local, ignored by Git).

- Initial NDK build exposed Fbox API differences; corrected.
- Subsequent build exhausted host memory. Building native targets with explicit
  `cmake --build ... --target xrRender_GL --parallel 2`, then Gradle packaging,
  completed. Compiler/linker temporary data is now also kept on R:.
- Final normal APK built successfully, profiling OFF, installed by adb install -r.
- Mesa GLES: **313 compile/link pairs, 143 RGBA8 pixel checks passed**
  (gles-adreno-fix.json), including actual rendered-atlas/HQ sampling, original
  alpha/fog/lighting formulas and distinct HQ/LQ SKIN_2 bone choices. This is
  software-driver evidence, not Adreno visual parity.
- Galaxy Tab S9 / SM-X710 / Android16 / Adreno740, dedicated Cordon save:
  projector receivers=3, updated=3, **lighting_surface_draws=50**, pending=0;
  isolated models **HQ=3, LQ=15, draws=18**.
- Source c84558d exposed uninitialized cache pixels; after 7280d31 all three
  tiles have nonzero computed RGB. Slot 2 changed from (0,0,0,0) to (8,8,8,6),
  matching actual ROS ambient rather than a synthetic brightness adjustment.
- Final component test has no GLSL compilation failures and reports
  **deferred_calls=0**, unsupported=21, blocked=1, full-world native_draws=0.
  Those 18 isolated model draws and 50 atlas draws are additional diagnostic
  component draws, not playable full-world or performance evidence.
- User config and all existing saved-game files match pre-test hashes after
  component testing. 18 original game archives remain in place, not recopied.

R2 and preserved-hybrid regression reruns on this APK both passed their bounded
Cordon load/look/move checks. Three screenshots per mode show the changed
camera and position, and logs contain no shader compilation failures or fatal
errors. This is a short smoke test, not full game coverage or a performance test.
All pre-existing user config/save hashes remain identical after all tests.
The pulled installed APK matches the saved SHA-256 exactly. Screen is off and
game closed. Maximum battery temperature across 53 samples was 22.3 C; maximum
Android thermal status was 0. These are not processor temperature measurements.

CI for code 7280d31 passed the GLES step but failed before native compilation:
https://github.com/thithertom39/OLR-ANDROID/actions/runs/45068979134.
Three attempts hit HTTP 429 from Maven Central while resolving pinned AGP
dependencies. Local APK builds succeeded. A CI-only init script now reroutes
only Central downloads through the Google Cloud hosted mirror documented at
https://docs.cloud.google.com/docs/buildpacks/java#configure_maven. Dependency
versions, the Android Maven repository and shipped renderer are unchanged.
The mirror init script passed local Gradle configuration/dependency resolution
(`help`, 55 seconds). New CI for unchanged renderer code is running at
https://github.com/thithertom39/OLR-ANDROID/actions/runs/45070235018
(CI-only commit 7c2b219). It must be checked to completion; no successful CI APK
is claimed for this checkpoint yet.

## Saved APK / installation / diagnostics

Regular APK: artifacts/OLR-R1-model-projector-7280d31-20261122.apk.
SHA-256: E1427D37D80B3F85F0E098471ADFF45DD1CC1D945783F5CB366A98D982B79DC9.

```powershell
& 'C:\Users\User\Documents\Codex\2026-10-11\gjvyb\work\android-sdk\platform-tools\adb.exe' install -r 'R:\project\OLR-R1-parity\artifacts\OLR-R1-model-projector-7280d31-20261122.apk'
```

Diagnostic launch arguments (incomplete independent mode, returns to menu):

```text
-olr_r1_forward -olr_r1_multipass -olr_r1_native -olr_r1_scene -olr_r1_independent -olr_r1_projector_capture -start server(r1_checkpoint_20261121/single/alife/load) client(localhost)
```

Use tools/tablet-r1-smoke.ps1 for bounded sessions with battery/thermal checks
and force-stop + screen off in finally. Battery temperature is not CPU temperature.
No wake locks, power settings, thermal limits, brightness or user saves changed.

Old stable/control APKs are preserved, including
artifacts/OLR-R1-independent-checkpoint-20261122.apk (C17546CC...07C).
CI .dev APK uses a different signing key; do not install it over the regular app.

## Still incomplete — next implementation

1. Actual original object-local-light/LightPPA traversal and submission. Model
   POINT/SPOT shader permutations exist, but are not a complete runtime path.
   Also verify original additive alpha/color-write behavior: OLR add_point/spot
   returns RGBA final_color*2, whereas the current shared native local PS writes
   zero alpha/RGB-only. Do not claim full alpha compositing parity from RGB tests.
2. Original LightShadows caster selection, model silhouettes, blur, collision
   receiver clipping/cache and world projection. Do not substitute R2 shadows.
3. HUD/NPC/weapon full-scene checks, projector cache reuse/movement/expiry,
   indoor/outdoor/night/multiple-light controls and PC visual comparison.
4. Model environment materials, special Lua, emissive, water, wallmarks,
   distortion and any other unported inventory family. The frame guard remains.
5. Only after complete independent R1: CPU/GPU and Full HD performance work.
   No new FPS improvement, 60 FPS or full visual-parity claim is made here.

Do not mark models or the full independent renderer complete based on this
component image. Preserve original lighting formulas and previous controls.
