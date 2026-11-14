# Brightness screenshot A/B

No obvious global increase reproduced in these sampled static regions; this does not establish pixel identity or whole-game correctness.

Parent: ragdoll5 without Skadovsk patches, `walk-20261031-041442/after.png`; candidate: initial `diag-bench-before.png`. Both show HUD09:25, same distant scene and no console. Weapon/wind differ.

| Region | Parent encoded Y | Candidate encoded Y | Change |
|---|---:|---:|---:|
| sky_center | 152.785 | 154.328 | +1.010% |
| sky_right | 188.681 | 189.621 | +0.498% |
| ship_bridge_upper | 105.473 | 105.394 | -0.075% |
| ship_hull_clear | 86.253 | 86.255 | +0.002% |
| terrain_left | 115.915 | 116.040 | +0.108% |
| terrain_gap | 105.806 | 105.978 | +0.163% |

Compared 103 common logged graphics-setting values; differences: {}. These logs are context, not screenshot-instant telemetry.

Regions exclude UI/weapon/near grass and are recorded exactly in JSON. Sky varies slightly with cloud phase; static ship and terrain show only small changes. No broad luminance jump is visible or measured in these regions.

This is a screenshot brightness proxy, not physical luminance. Exact fractional time, dynamic cloud/wind and exposure state are not matched. No previous-day user reference is available. Pixel identity, all lighting correctness and performance equivalence are not established. No ADB, production or input-image changes were made.
