# Original R1 reference findings — 2026-11-21

This is a source/data comparison, not proof of image parity on Android or proof
that the proprietary OLR 2.5 PC renderer DLL is byte-for-byte equivalent to the
public X-Ray reference. The local OLR shader sources and compiled level data were
read from `R:/project/unpack/all`. No third-party game resources are included in
this document or added to the repository.

The Android comparison baseline was PR #7 at
`1137caad2485411e26b8fb9ece4ea7de9d8277ac`. Public reference URLs below name the
`OpenXRay/xray` master branch as read on 2026-11-21. Their resolved commit SHA was
not recorded during this comparison; these URLs must not be described as pinned
references.

## Correction to the previous source comparison

The material-specific findings here **supersede the assumption in
`OLR25_R1_SOURCE_COMPARISON.md` that `impl.ps` / `impl_dt.ps` describe ordinary
`default` lightmapped level geometry**. The equations previously quoted from
those files exist, but those files are not the shaders selected by the original
`B_DEFAULT` blender.

Evidence from the actual local game resources:

- `shaders.xr`, outer chunk 2, subchunk 22: name `default`, class ID
  `make_clsid("LM      ")`, which is `B_DEFAULT` in `Blender_CLSID.h`.
- `levels/lvl1_escape/level`, shader-list chunk 2, contains entries such as
  `default/wood\\wood_walls1,lmap#98_1,lmap#98_2`. There are 1,235 shader-list
  entries in that level.
- The public original R1
  [FStaticRender_Loader.cpp](https://github.com/OpenXRay/xray/blob/master/trunk/xr_3da/xrRender_R1/FStaticRender_Loader.cpp)
  passes the texture-list string to resource creation directly.
- Original
  [BlenderDefault.cpp](https://github.com/OpenXRay/xray/blob/master/trunk/xr_3da/xrRender_R1/BlenderDefault.cpp)
  selects `lmap` / `lmap_dt` for the normal pass and binds texture-list index 1
  to `s_lmap` and index 2 to `s_hemi`. There is no `_1` / `_2` remapping in this
  loader/blender chain.

The matching local OLR `r1/lmap.ps` equation is:

```text
light = L_ambient / 1.5
      + sample(s_lmap).rgb
      + vertex_sun * sample(s_lmap).a
      + vertex_hemi * mean(sample(s_hemi).rgb)
result.rgb = 2 * base.rgb * light
result.a = base.a
```

`lmap.vs` obtains UVs for both lightmap samplers from `unpack_tc_lmap(v.uv1)`.
`lmap_dt.ps` multiplies the base result by `2 * detail`, with the original
distance-faded detail expression. In contrast, `impl.vs` uses base UVs for its
second texture and `impl.ps` modulates hemisphere by base alpha. These are
different material models and should not be interchanged.

The original additional `add_point.ps` / `add_spot.ps` passes do not sample a
detail texture. Applying detail modulation to those passes is a further
material-specific difference.

## POINT and SPOT equations and transforms

Primary C++ source:
[LightPPA.cpp](https://github.com/OpenXRay/xray/blob/master/trunk/xr_3da/xrRender_R1/LightPPA.cpp).
Local OLR shader references: `r1/common.h`, `lmap_point.vs`, `lmap_spot.vs`,
`add_point.ps`, `add_spot.ps`, and `r1/.s`.

POINT parameters and coordinates:

```text
L_dynamic_pos.w = 0.5 / range
tc = (world_position - light_position) * L_dynamic_pos.w + 0.5
attenuation = sample(internal_light_attpoint, tc.xz)
            * sample(internal_light_attpoint, tc.xy)
```

Both lookups use linear filtering and clamp addressing. There is no inverse
square or analytic quadratic distance formula in these shader passes.

SPOT uses a view built from the normalized light direction. The initial up
vector is `(0,1,0)`, changed to `(0,0,1)` if its absolute dot product with the
direction is greater than `.99`. The cross-product basis is normalized. The
projection uses the authored cone, aspect 1, near plane `.1`, and far plane
`range + EPS_S`.

```text
tex_adjust scale       = (0.5, -0.5, 1 / range)
tex_adjust translation = (0.5 + 0.5 / 32, 0.5 + 0.5 / 32, 0)
light_xform = tex_adjust * light_projection * light_view
s = light_xform * world_position
projected_uv = s.xy / s.w
attenuation_uv = (s.z, s.z)
```

`32` is the original C++ `SSM_tex_size` constant. It is not derived from the
actual lookup image dimensions. The Z attenuation coordinate is not divided by
W: with the original LH projection it is `Q * (view_z - .1) / range`, where
`Q = far / (far - near)`. Replacing this with Euclidean distance or simply
`view_z / range` changes the original behavior. The current shared
`Fmatrix::build_projection` is also LH with D3D-style depth range.

The SPOT samplers are fixed `internal_light_att` (projected) and
`internal_light_attclip` (linear/clamp). Original R1
[light.cpp](https://github.com/OpenXRay/xray/blob/master/trunk/xr_3da/xrRender/light.cpp)
implements `set_texture(name)` as an empty method under `RENDER == R_R1`.
Arbitrary R2 light cookies therefore must not automatically be treated as a
feature of this original R1 reference.

The original light vertex color includes the surface/light dot product and
`saturate(calc_fogging(world_position))`. The pixel pass multiplies base,
lookup samples and light color, additionally modulates RGB by base alpha, and
applies the final factor of 2. Additive blending is ONE + ONE.

D3D PS1.x input COLOR registers are clamped to `[0,1]`, unlike ordinary GLSL
varyings. This is documented by
[Microsoft's PS1.x register reference](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-ps-registers-ps-1-x)
and
[Writing HLSL Shaders in Direct3D 9](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-writing-shaders-9).
The documentation establishes clamping of the interpolated shader input;
clamping every vertex before interpolation is not separately established by
that evidence.

The camera-distance light scale is:

```text
distance_to_volume = distance(camera, light.spatial.sphere.center)
                   - light.spatial.sphere.radius
scale = clamp(1 - distance_to_volume / r1_dlights_clip, 0, 1)
```

Using `position` and `range` unconditionally is not equivalent for a SPOT light,
whose spatial bounding sphere can have a different center and radius.

## Lookup resources are available locally

The local `textures/internal` directory contains the original resources:

| Resource | Dimensions | DDS encoding | File size |
| --- | --- | --- | --- |
| `internal_light_att.dds` | 64 x 64 | Uncompressed | 16,512 bytes |
| `internal_light_attpoint.dds` | 64 x 64 | Uncompressed | 16,512 bytes |
| `internal_light_attclip.dds` | 128 x 2 | Uncompressed | 1,152 bytes |

Their existence removes the need to invent lookup curves or find a runtime
texture generator. Pixel masks, upload swizzle and sampler binding still need
verification in the Android implementation. These DDS files are game assets,
not candidates for committing to the public source repository.

## Sector/subspace traversal

Primary source:
[r__dsgraph_render.cpp](https://github.com/OpenXRay/xray/blob/master/trunk/xr_3da/xrRender/r__dsgraph_render.cpp),
`r_dsgraph_render_subspace`, together with `LightPPA.cpp`.

The original traversal increments the renderer marker, creates a frustum with
all planes, saves the current view, and traverses portals from the light's
sector. It submits each reached sector's static root through its portal
frustums. Dynamic renderables are queried from the spatial database and then
tested against the reached-sector marker and portal frustums. This is more
restrictive than a camera-visible mesh/light bounding-sphere intersection.

SPOT requests precise portal handling: nearby portals found by a collision box
of half-size `EPS_L * 20` around the light COP receive DualRender. The SPOT
frustum uses the same view/projection described above. POINT uses an unusual
selection frustum: COP is `light.position + (0,300,0)`, direction is downward,
up is positive Z, FOV is 2 degrees, and near/far are `300-range` and
`300+range`. The starting sector is still the light's sector.

Both paths include dynamic geometry. They render graph 0 and additionally HUD
geometry if the light frustum intersects a sphere of radius 2 at the camera.
The saved view is restored afterward. Sector selection is not proof of
per-pixel shadowing through every wall within the same sector.

## Color composition

[FStaticRender_RenderTarget.cpp](https://github.com/OpenXRay/xray/blob/master/trunk/xr_3da/xrRender_R1/FStaticRender_RenderTarget.cpp)
creates the scene target in `HW.Caps.fTarget`, or renders directly to the base
target when the R1 postprocess is unnecessary.
[HW.cpp](https://github.com/OpenXRay/xray/blob/master/trunk/xr_3da/HW.cpp) selects
8-bit UNORM RGB/RGBA formats for the 32-bpp path. The R1 frame path does not
perform R2 exposure adaptation or HDR tone mapping. Additional light colors
are blended directly into the LDR target. The final R1 effects include game
postprocess, noise and duality; they are not an exposure/tone-map stage.

Consequently, a tone-mapped difference composed into an already tone-mapped R2
frame cannot be claimed to reproduce this original composition. Retaining the
old approximation as a fallback is distinct from implementing the R1 path.

## Compiled Cordon lights and the basement fixture

`lvl1_escape/level` chunk 6 contains 14 records of 108 bytes: a 4-byte controller
followed by the 104-byte `Flight` structure. Record 0 is directional; the
remaining records are point lights in the compiled data. Record 13 is at
approximately `(-244.6303,-9.082104,-134.4171)`, range 5, RGB approximately
`(.35765,.38902,.4)`.

That underground position is a candidate for investigating the basement, not
a confirmed identification of the user's problematic fixture. The level-light
loader marks compiled lights `bStatic`, and original programmable R1 excludes
them from additive lighting because their contribution is baked. A separate
spawned hanging-lamp entity can still own a runtime light. Identifying that
entity and matching the test camera is required before changing its behavior.

## Material coverage that remains to establish or port

| Material group | Original behavior requiring a matching implementation |
| --- | --- |
| Vertex-lit geometry | Vertex RGB + hemisphere times normal alpha + sun times vertex-color alpha + ambient; diffuse times 2 |
| Models LQ / NPC / weapon / hands | Per-object ROS hemisphere and sun lighting, skinning and material alpha; diffuse times 2 |
| Models HQ | `LightProjector` projected lightmap, blending with LQ using projector constants |
| Dynamic shadows | Separate original `LightShadows` / `LightProjector` system; not a blanket copy of R2 shadow maps |
| Trees | `c_scale` / `c_bias` decompression, hemisphere wrap, separate sun factors, wind deformation |
| Detail grass | Instance/CPU light color and separate wave/still vertex shaders |
| Alpha-tested / transparent / emissive | Material-specific blender/Lua states and light elements |
| OLR water | Authored Lua selects specialized shaders and blends; `effects_water.s` selects `waternew`, SRCALPHA + SRCALPHA and a position texture |
| Special OLR effects | Individual original Lua/material shader behavior; no generic R2 substitution establishes parity |

In particular, porting the standalone `water.ps` is insufficient evidence of
support for the material actually selected by `effects_water.s`.

No source files, APKs or tablet state were changed while gathering these
reference findings. Runtime shader compilation, actual texture upload, visual
matching, source-to-fixture mapping and performance require separate tests.
