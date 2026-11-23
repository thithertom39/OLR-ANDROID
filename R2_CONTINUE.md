# R2 final optimization — 23.11.2026

User explicitly froze unfinished R1 and switched to R2; do not resume R1.
R1 freeze commit f36351c55cfc7ed3080931137bd0814dc6583617, PR #8;
resumption file R:\project\OLR-R1-parity\R1_FROZEN_FOR_R2_20261123.md.

R2 worktree: R:\project\OLR-R2-final, branch perf/olr-r2-cordon-20261123.
Stable base origin/main 4b07c0e, renderer source identical to initial 8ad6590
(intervening four commits only README). No experimental R1 commits imported.
097678e restores only the missing upstream LuaJIT host CMake definition.
Pinned Vorbis lib 0657aee restored locally from upstream (snapshot ignores lib/).

Baseline native profiled build is running with jobs=2; see logs/bootstrap.
Existing profiler enabled by -gles_gpu_profile, CPU split by -gles_cpu_split,
scene counters by -gles_scene_compare, all optional, no R1 launch arguments.
Control save: r1_checkpoint_20261121 (name historical; normal R2 can load it).
Device assets /sdcard/Download/STALKER-OLR2.5. Preserve all archives and saves.
Before hashes/user.ltx backup: logs/device-before. Existing signing key reused
locally (.android/debug.keystore ignored), never publish it.

First measure plain R2 at 1728x1080 before optimizing; then compare fixes and
test 1920x1080. Existing launcher already has exact Full HD preset. Settings
must match A/B and be restored after tests. No graphics-quality reductions.
Read-only KGSL gpubusy/frequency snapshots are available; these are recent
driver windows, not cumulative per-frame GPU execution. Perfetto query did not
advertise GPU render-stage/counter sources. Do not claim timestamps are busy time.

Close game and screen off during builds/analysis and after every short session.
Stop at battery >=40 C or Android thermal >=3. Never change thermal/performance
system controls. Finish meaningful R2 fixes/checks, save final APK/report, stop.
