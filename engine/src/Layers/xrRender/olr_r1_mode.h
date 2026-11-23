#pragma once

// Retain both earlier experiments for reproducible A/B comparisons.
// This is a progressive R1 material path, not a claim that all of R1 is ported.
inline bool olr_r1_native_enabled()
{
    static const bool enabled = Core.Params && strstr(Core.Params, "-olr ") &&
        strstr(Core.Params, "-olr_r1_forward") && strstr(Core.Params, "-olr_r1_multipass") &&
        strstr(Core.Params, "-olr_r1_native");
    return enabled;
}

// Progressive scene-material port. Keep the 2026-11-21 hybrid selectable for A/B.
inline bool olr_r1_scene_enabled()
{
    static const bool enabled = olr_r1_native_enabled() && strstr(Core.Params, "-olr_r1_scene");
    return enabled;
}
