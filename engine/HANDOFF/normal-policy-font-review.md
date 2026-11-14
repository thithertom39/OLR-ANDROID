# Independent normal-policy and font/upload review

Source review: GREEN within the stated narrow scope. No device access or production changes; existing tests were not rerun.

Normal policy: only the option selector differs from reviewed skad3diag. Non-null parameters default ON; exact `-gles_no_detail_light_cull` wins; null parameters disable. Re-evaluating arguments avoids stale same-process session state. Geometry header remains byte-identical at `7b0aaa6a050d03cf05c02b78f48f3e7dbab1651935eed10b69d8793930078057`. Existing 398-case policy/lifecycle optimized and sanitizer evidence and two actual ARM64 translation units with profiler macro0 were verified.

Font: confirmed source defect in the batch estimate: `smart_strlen(i+count)` was paired with action-expansion length of `i`. The one-line correction uses `i+count` consistently. Extracted actual-source regression reserves108/emits168 before, reserves168/emits168 after. Existing ordinary batches stay unchanged. Map/Unmap bounds and exact upload range are retained; additional logs run only on failure.

The 04:38 runtime fatal caller is still unconfirmed. The font overrun log is after glyph emission and does not constitute a new pre-write guard. Tests cover the actual estimator/action helper and Unmap with counted GL adapters, not the full live font GPU path. ASCII binding fixtures and syntax checks do not prove every UI state. Final normal APK build and hardware validation remain separate.

Evidence/status/log/input hashes and full frozen source pins are recorded in `normal-policy-font-review.json`.

Candidate `detail-cull-production-candidate` manifest `685b97c202632ac2d8059a1b45bbe150d28456c91ce3e0c7b00cda1428754d85`.
- `C:\Users\User\Documents\Codex\2026-10-11\gjvyb\work\skadovsk-night-20261031\detail-cull-production-candidate\tests\results.json`: `d126298ba72bf4a9f97794b89813f4fd6c28ba60abc2afdfae808f2f265f5096` (6 recorded commands verified).

Candidate `vertex-upload-candidate` manifest `f0d78ad7afb87fc4d4afd13b96bfb0b8b97724c271719236a33ee58a96657e6f`.
- `C:\Users\User\Documents\Codex\2026-10-11\gjvyb\work\skadovsk-night-20261031\vertex-upload-candidate\tests\results.json`: `0d8ca9bd17ac6aee7483dbbe9d6c1ecd49c2408fcf69b9c126335025dacbc830` (24 recorded commands verified).
- `C:\Users\User\Documents\Codex\2026-10-11\gjvyb\work\skadovsk-night-20261031\vertex-upload-candidate\tests\ndk-results.json`: `c2fe9db566466f071cfa5b59a5d3ff7d95a8268a2c14527ab07c131bdc7791b1` (2 recorded commands verified).
