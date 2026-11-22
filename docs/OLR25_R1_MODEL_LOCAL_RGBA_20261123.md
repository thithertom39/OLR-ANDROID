# Model local-light RGBA checkpoint, 23 November 2026

Code: `3576b19ea2e83399b9f175ad57d9ed6104ff51a9`.
Branch and draft PR #8 unchanged. Full R1 goal remains active and incomplete.
Prior component checkpoint: OLR25_R1_MODELS_PROJECTOR_20261122.md.

## Implemented from original sources

Original OLR `r1/add_point.ps`, `add_spot.ps`, `common.h` and reference
`Blender_Model.cpp` establish RGBA final_color*2, with material alpha additionally
modulating RGB. Local COLOR is RGBA N.L*fog. C++ opaque local passes enable
alpha testing at reference 0; blended models use their authored reference.
Alpha testing is after the pixel shader, before blending/target writes
(https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dusage-query).

- Model local VS now preserves COLOR alpha as well as RGB.
- Model POINT/SPOT PS uses original texture-alpha products and doubles RGBA.
- Local model passes enable all four color-write channels, ONE+ONE, depth equal.
- Alpha tests compare final local output, including attenuation/fog, rather
  than only diffuse alpha; opaque local passes also reject final alpha zero.
- Existing shared non-model/hybrid local behavior is unchanged. The new GLSL
  branch is enabled only by model-local wrappers, not a global renderer option.
- No exposure, ambient, gamma, light-count cap or new lighting coefficients.

Modified: olr_r1_model.vs, olr_r1_model_point.ps, olr_r1_model_spot.ps,
olr_r1_native.ps, olrR1Models.cpp and tests/r1_model_probe.py.

## Verified and limits

- Full GLES suite after RGBA/final-alpha changes: 313 compile/link pairs and
  149 reference pixel draws pass. After the model-only opaque reference-0
  correction, all 51 model/projector pairs and 27 relevant pixel cases pass.
  Those cases include positive RGB with zero lookup alpha (must discard),
  final-output alpha cutoffs, material/lookup alpha products, fog, and absence
  of an active s_position/G-buffer sampler.
- NDK build and final regular APK packaging succeeded.
- APK installed with adb install -r on physical SM-X710; short Cordon diagnostic
  completed with no shader compile/link failures or forbidden R2 stage.
- Atlas: 3 receivers, 50 surface draws; isolated models: 3 HQ + 15 LQ / 18 draws.
  Full independent frame remains blocked, unsupported=21, deferred_calls=0.
- This device check compiles the new model programs, but does NOT establish
  actual local-light draws on models: LightPPA runtime submission is still
  missing. RGBA pixel math is verified by software GLES fixtures; real-game
  light/cookie/occlusion/alpha tests remain required after integration.
- R2 and hybrid look/move regression tests were passed on the preserved
  7280d31 control APK. They were not repeated after this isolated R1-model
  change; legacy/non-model GLSL remained covered by the full suite above.
- No FPS claim or independent full-scene parity claim is made.

## APK and CI

Installed APK: artifacts/OLR-R1-model-local-rgba-3576b19-20261123.apk.
SHA256: E5834B211601F9DC527AE7FADE795D49E9ADD2B2DBAC24021F8CE451ACA5FE6B.

```powershell
& 'C:\Users\User\Documents\Codex\2026-10-11\gjvyb\work\android-sdk\platform-tools\adb.exe' install -r 'R:\project\OLR-R1-parity\artifacts\OLR-R1-model-local-rgba-3576b19-20261123.apk'
```

Prior successful CI, renderer code 7280d31 / CI-only transport commit 7c2b219:
https://github.com/thithertom39/OLR-ANDROID/actions/runs/45070235018.
Artifact 14772999873 downloaded under artifacts/CI-7c2b219; ZIP SHA256 verified
against GitHub: a181c7a2d1a5361a3a72408a3dd5b093ff2113069f3300a5a03a7b04f8d3df1d.
CI .dev APK SHA256 E63F9D2E70455967431C10A3F87F95EB35EB5F5662C02BD99B6A599B6E564BD5.
It was NOT installed over the regular app (different signing key).

New CI for 3576b19 SUCCEEDED:
https://github.com/thithertom39/OLR-ANDROID/actions/runs/45074072056.
It independently passed NDK compilation and the full GLES suite: **313 pairs,
151 pixel fixtures**, including the final opaque reference-0 correction.
Artifact 14774823062 / OLR-Android-arm64-24 is saved in artifacts/CI-3576b19.
ZIP SHA256 8d338ea930e22e1febb6d14921b2d999e27ff27d35e0118a152bac7f4cd03dc8
matches GitHub's published artifact digest. CI .dev APK SHA256:
8A024ACD1AB4CFAB64A83BDD9103E3C9DA5752E0AA22D7222788BE97F5EDAA18.
This CI APK was not installed over the regular, device-tested APK.

Logs: logs/20261122/models-projector. Final installed hash, saved-game/config
hashes and screen/thermal state: rgba-final-integrity.json in that directory.
Previous APKs and the original project remain intact; no game archives recopied.

## Next concrete implementation

1. Finish original LightPPA object-light selection via sectors/precise portals
   and light frustums, using actual spatial owners already carried from common
   visibility. A sphere intersection alone is insufficient. Preserve SPOT/POINT
   passes, r1_dlights_clip, source matrices/lookups, static exclusion/dedup and
   per-draw RGBA8 saturation. No arbitrary light cap.
2. Integrate these model-local programs in world and HUD draw submission, then
   test actual lamps/torch, intersections and occlusion on the tablet.
3. Original LightShadows and remaining material families. Keep coverage guard
   until full required behavior works; do not whitelist models based on base
   shaders alone. Projector movement/cache reuse/expiry also remain unverified.
4. Independent full-scene PC parity and performance work remain later stages.
