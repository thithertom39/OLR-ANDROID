# Native model LightPPA — 23 November 2026

Full independent R1 is still incomplete. This checkpoint records a real component
integration, not a claim of complete gameplay or PC image parity.

## Source and implementation

- `64b58f2fb130b0804ccc5e49829bb65aba033a3c`: original LightPPA model submission.
- `e04093b188d746a7b14a753a8ac5dac86ba499f2`: opt-in physical pixel diagnostic and HUD winding.
- Reference: OpenXRay/xray d7b2359 `xrRender_R1/LightPPA.cpp`, and original OLR
  `r1/common.h`, `model_def_point.vs`, `model_def_spot.vs`, `add_point.ps`, `add_spot.ps`.

Camera-visible base model submissions retain the same world matrices and SSA LOD.
Each light gets its original visibility camera: POINT top-down from Y+300 at
2 degrees, SPOT cone projection with near .1. Selection uses a separate CPU
sector/portal context, precise SPOT portal handling, the real renderable owner,
root spatial sphere and transformed leaf sphere in the clipped sector frustums.
No sphere-only replacement, R2 shadow atlas, light cap or merged light sum.

Original camera-to-light-sphere r1_dlights_clip fade, PR constants, 32-texel
SPOT texel adjustment, DDS lookups, material alpha and final-alpha tests remain.
World local passes are light-centric, SPOT before POINT, RGBA8 ONE+ONE and
depth equal. HUD local submission is connected inside the common HUD transform;
left-handed culling is restored after setting each local material.
Invisible/unsupported materials are not whitelisted by these changes.

## Physical SM-X710 evidence

Two 70-second sessions, app force-stopped and screen off afterwards:

1. `ppa-initial`: 18 models, 4 real level lights, 1 accepted world pair,
   71 frustum rejects, one actual POINT draw. The base/local image delta was
   **zero**. This does not prove a lighting contribution at that saved view.
2. `ppa-fixture`: an explicitly synthetic, unregistered camera light fixture
   exercised the same real traversal, model programs and original DDS textures.
   Total six lights / five pairs, 3 POINT + 2 SPOT submissions. Per-light
   glReadPixels confirmed fixture SPOT **38625** brighter pixels and fixture
   POINT **38626**, zero darker pixels. Original scene lights still gave zero
   visible delta. Fixture colors/positions are test inputs, never replacement
   game-light coefficients or a claim about the original basement lamp.

Both: 3 HQ + 15 LQ base models, 18 base draws; independent frame blocked by
coverage guard with unsupported=21 and deferred_calls=0. No new GLSL/link/fatal
failure. Existing unrelated script documentation SYNTAX ERROR lines remain.
Maximum sampled battery 22.1 C, thermal status 0. Battery is not CPU temperature.

Logs and raw base/local RGBA readbacks: `logs/20261123/lightppa/`.
Actual HUD, occluded indoor/portal scenes, moving light/cache reuse, full-game
lighting and PC parity still require tests after the remaining frame paths work.
No FPS claim is made.

## Builds

Regular APK (device tested, before shadow work):
`artifacts/OLR-R1-model-lightppa-e04093b-20261123.apk`.
SHA256 `53D8F028682326E2090C8896DD75A4E057588CF3C8A0373E2338536A91403230`.

64b58f2 CI succeeded: https://github.com/thithertom39/OLR-ANDROID/actions/runs/45099300915 .
Downloaded artifact digest verified, `artifacts/CI-64b58f2/verified.json`;
313 shader pairs / 151 pixel fixtures, zero pixel failures.
e04093b CI succeeded: https://github.com/thithertom39/OLR-ANDROID/actions/runs/45100782274 .
CI APKs have a different signing key and were not installed over the regular app.

Normal independent flags (coverage guard still blocks incomplete scenes):
`-olr_r1_forward -olr_r1_multipass -olr_r1_native -olr_r1_scene -olr_r1_independent`.
One-off component diagnostic additionally uses `-olr_r1_projector_capture
-olr_r1_model_light_probe`. Without these options no fixture or readback exists.

## Continue

Original LightShadows: caster eligibility and original atlas, blur, receiver
collision/frustum clipping, cached triangulation, PLC_energy and projected
multiplicative world pass. Then remaining model-environment, emissive, sorted
Lua/water and other families. Preserve the full independent R1 scope and guard.
