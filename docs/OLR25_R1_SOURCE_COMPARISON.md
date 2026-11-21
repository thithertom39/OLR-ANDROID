# OLR 2.5 PC R1 vs Android GLES forward experiment

> Historical comparison, corrected on 2026-11-21: the `impl.ps` observations
> below are valid for that shader, but applying them to the level's `default`
> materials was incorrect. Actual `shaders.xr`, level material strings and
> `BlenderDefault` identify `default` as **lmap/lmap_dt**, using the second
> hemisphere map and ambient/1.5. See `OLR25_R1_REFERENCE_20261121.md` and
> `OLR25_R1_GLES_STATUS_20261121.md`. The old multipass remains an A/B baseline.

This is a **source comparison**, not a claim of visual parity. The user supplied
`shaders.zip` (unpacked original OLR 2.5 shaders) and `lvl1_escape.zip`
(one full level directory) on 2026-11-18. **These third-party game assets
are not copied into this public repository.** Only observations appear here.

## Confirmed by original OLR shaders

- `shaders/r1/common.h`: OLR's authored vertex hemisphere formula is
  `L_hemi_color * (0.98 + 0.2 * normal.y)`, and the sun formula uses a
  `max(0.3, dot(...))` floor. These are already present in our GLES VS.
- `shaders/r1/impl.ps` and `impl_dt.ps` (ordinary static lit materials):
  `light = L_ambient + lmap.rgb + vertex_sun * lmap.a + vertex_hemi * base_texture.a`.
  The illumination is multiplied by base RGB and by 2, with another detail
  modulation in `impl_dt.ps`. The separate `s_hemi` sampler approach used
  by our older forward experiment **is not the same formula**.
- `shaders/r1/add_point.ps`: per-light color is base * two *sampled*
  attenuation textures * vertex light color, with additional multiplication
  of RGB by base alpha and a final 2x scale.
- `shaders/r1/add_spot.ps`: per-light color uses base * projected-light
  sampler * attenuation/clip sampler * vertex light color, also modulated
  by base alpha and doubled.
- `shaders/r1/lmap_point.vs`, `lmap_spot.vs`, and `r1/common.h`:
  the original R1 supplies UVs / projections from the light's transform,
  world position, normalized surface normal, light position, and range.
- `shaders/r1/.s`: per-light passes use ONE + ONE additive blending and
  depth testing with depth writes disabled. Original attenuation resources
  are named `internal\\internal_light_attpoint`,
  `internal\\internal_light_att`, and
  `internal\\internal_light_attclip`. Their **actual textures / generation
  paths were not included** in this shader-only archive.

## Confirmed by level files

`lvl1_escape.zip` contains the `lvl1_escape` directory with the
compiled `level`, geometry, vegetation, and 241 files named `lmap#*.dds`.
Examined sample lightmap images are DXT5 1024×1024. The paired `_1` /
`_2` textures carry different channels, including lightmap alpha and
hemispheric/occlusion-related data. RGB in several sampled `_1` textures
is nearly black while the corresponding `_2` contains nonzero RGB; this
**does not prove data loss**.

The root `level` binary has an uncompressed chunk ID 6
(`fsL_LIGHT_DYNAMIC`, as defined in `Common/LevelStructure.hpp`) of
1512 bytes. The presence of this chunk verifies compiled level light data,
but the exact bulb in the tested basement has **not** been identified from
that chunk. `level.ltx` contains only a minimal comment. The level archive
does not contain a `build.lights` file.

## Changes applied to opt-in R1 multipass

- Baked light on ordinary static materials now uses the original
  `r1/impl.ps` ambient and alpha-modulated hemisphere formula.
- Separate dynamic POINT and SPOT contributions now also use the base
  texture alpha, as in the original `r1/add_point.ps` /
  `add_spot.ps`.
- All changes are confined to `-olr_r1_forward -olr_r1_multipass`.
  The older opt-in single-pass `-olr_r1_forward` remains unchanged.

## Still NOT reproduced

1. Exact point-light attenuation textures / coordinate projections.
2. Exact spotlight projected image, attenuation and clip mask.
3. Original dynamic-light material and geometry pass scheduling, sun,
   vertex-lit and alpha-tested coverage, and projected-shadow behavior.
4. Direct original-R1 output pipeline independent of remaining R2
   composition and tone mapping.
5. Device-level shader compilation and visual/performance parity for the
   latest source changes; successful NDK/Gradle builds do not establish it.

**Do not replace baked lightmaps or add all compiled static lights as
runtime dynamic lights without evidence**: that would double-count baked
contributions and obscure the source of the basement light discrepancy.

For further exact light-matching, identify the engine's source/generation of
`internal_light_attpoint`, `internal_light_att`,
`internal_light_attclip`, and how OLR 2.5 loads them. If they exist as
actual textures in the unpacked game's `textures/internal`, use those
specific files as references; otherwise read their runtime generator.
