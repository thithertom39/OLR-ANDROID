# OLR R1 GLES: implementation and test boundary, 2026-11-21

This branch continues PR #7 at `1137caa`. It does **not** implement the whole
original R1 renderer yet. Previous forward/multipass and ordinary R2 remain
selectable; no game textures, levels or original shader archives are published.

## Implemented native material path

Enable on the OLR game with all three arguments:

```text
-olr_r1_forward -olr_r1_multipass -olr_r1_native
```

The launcher supplies `-olr` itself. Use Native OpenGL ES. Do not select
"No shadows (-nodlights)": that existing launcher option removes dynamic
sources altogether, including the flashlight.

- Actual `default -> B_DEFAULT -> lmap/lmap_dt` mapping, original packed base
  and lightmap UVs, `_1` RGB/sun alpha and `_2` mean RGB hemisphere. Ambient
  is divided by 1.5 for this material. `impl` belongs to a different material.
- Static vertex-lit surfaces use vertex RGB, normal alpha hemisphere,
  vertex-color alpha sunlight and ambient, as original `vert`.
- Diffuse details and original distance fade apply to the base only.
- POINT uses two clamp/linear samples of the original
  `internal/internal_light_attpoint`, with `.5/range` and XZ/XY coordinates.
- SPOT uses `internal/internal_light_att` and `internal_light_attclip`,
  original D3D-LH projection, negative texture Y, authored `.5/32` offset,
  homogeneous XY divide and **undivided** transformed Z for clipping.
- Local diffuse/fog is evaluated in vertices, interpolated COLOR is clamped
  as PS1.x input, then multiplied by diffuse RGB, diffuse alpha and 2.
- No arbitrary light-count cap. Cubemap faces are deduplicated; compiled
  `bStatic` sources are excluded. Distance fade uses the spatial light sphere.
- Static local-light selection uses sector/portal traversal and clipped
  frustums in a separately allocated CPU context. Results are cached only
  within the frame, so moving/switched sources are reconsidered.
- Base and ONE+ONE / depth-equal local passes write directly to RGBA8.
  R2 exposure, HDR bloom, volumetric overlay, DOF and motion blur are bypassed
  in this mode. Final display gamma/game PP remain available.
- Both actual GLES Lua and C++ material compilers install the native elements.
  The generic `ResourceManager_Scripting.cpp` modified by the earlier PR is
  not built into `xrRender_GL`; its earlier behavior is preserved.

Original R1's `light::set_texture` in the reference is empty. Its SPOT projector
is the fixed internal texture above. Arbitrary R2 cookies are therefore **not**
claimed as original R1 behavior or implemented by inventing a cookie transform.

## Automated checks

```sh
LIBGL_ALWAYS_SOFTWARE=1 EGL_PLATFORM=surfaceless python3 tests/r1_gles_probe.py \
  --output logs/r1-gles-probe.json
```

The probe compiles and links the actual repository sources in GLES 3.2,
including all six material variants with diagnostic modes 0..6 (42 pairs).
It draws reference pixels into RGBA8 for lmap, detail, vertex, POINT, additive
saturation, SPOT XY/W vs undivided Z and behind-projector discard. Textures
in these isolated tests are synthetic; they do not establish real DDS upload,
Adreno correctness, complete scenes or visual PC parity.

2026-11-21 local result: Mesa 25.2.8 / llvmpipe LLVM 20.1.2, 42 pairs and
seven pixel checks passed. Local NDK/release APK build passed after correcting
the GLES compiler entry point. Physical-device and CI results are recorded
separately in the saved checkpoint report, without treating these host tests
as device evidence.

## Diagnostic display

Append `-olr_r1_debug=N` and restart the game. Normal mode is 0. These branches
are specialized by the shader compiler, included in cache keys and compiled
away in normal mode; there are no additional diagnostic samplers then.

| N | Native surfaces show |
|---|---|
| 0 | Final base + local-light composition |
| 1 | Baked light multiplied by diffuse |
| 2 | Hemisphere + ambient multiplied by diffuse |
| 3 | POINT contributions only |
| 4 | SPOT contributions only |
| 5 | Attenuation lookup products |
| 6 | Projected SPOT texture |

Diagnostic light images still use additive blending. Overlapping diagnostic
sources can saturate. Unported materials continue using their fallback, so
these are **not full-frame isolation views**.

## Tablet A/B procedure

Use the same save, camera, weather, time and graphics settings; load the same
save again before each mode. Preserve the original save and all 18 archives.
Install local APK with `adb install -r <APK>`. Cloud test APK uses `.olr.dev`
and its CI signing key, distinct from a locally signed regular installation.

| Mode | Additional arguments |
|---|---|
| Previous forward | `-olr_r1_forward` |
| Previous multipass | `-olr_r1_forward -olr_r1_multipass` |
| Native materials | `-olr_r1_forward -olr_r1_multipass -olr_r1_native` |
| R2 regression control | Remove all `-olr_r1_*` arguments |

Capture engine log, logcat and screenshot for each scenario:

1. Cordon basement lamp, flashlight off.
2. Closed room, flashlight off.
3. Same room and camera, flashlight on.
4. Several overlapping runtime sources.
5. Outside by day.
6. Outside by night.
7. NPC/moving object next to a source (fallback coverage must be reported).
8. Interior/exterior transition through portals.
9. ATP in sunny weather.

Each gameplay session must be short (at most 3-5 minutes), followed by force
stop and `adb shell input keyevent 223`. Keep the screen off during builds.
Read battery temperature and Android thermal status; stop at battery 40 C or
thermal status >=3. Battery temperature is not CPU/GPU temperature.

## Remaining material/architecture work

- Native skinned NPC, weapon and hand lighting (ROS values, original model
  LQ/HQ and `LightProjector`), and original R1 dynamic object shadows.
- Trees, grass/detail instances and LODs, with their original colors/wind.
- Alpha-tested, transparent, emissive and special OLR material states.
- Actual OLR `waternew`/water Lua behavior and its position/depth dependency.
- Dynamic-object light subspaces and original HUD local-light scheduling.
- Removing unused R2 G-buffer/light/shadow work after every material has a
  native replacement. Unported materials still require those stages now.
- Matching the exact cellar fixture/camera and comparable PC frames, all
  nine scenarios and sustained performance/heat behavior. No FPS target or
  full visual equivalence is claimed.

All three internal lookup DDS files exist in the supplied local game, so
their absence does not block this stage. The compiled Cordon light #13 is
documented in the reference; its identification as the pictured lamp remains
unproven. The read-only `tools/inspect_olr_lamps.py` searches validated spawn
packet headers and reports names/positions, without exporting game assets.
For exact OLR-specific engine deviations, the matching OLR 2.5 PC engine
source/version and a matching basement save/camera remain valuable evidence;
they are not replaced with guessed constants.
