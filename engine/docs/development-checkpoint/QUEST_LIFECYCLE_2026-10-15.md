# Quest lifecycle correction — 15 October 2026

Status: SOURCE_READY_APK_PENDING. This report supersedes any earlier claim that the complete cyclic quest lifecycle has been validated. Nightpolish remains the installed baseline; daypolish is a separate built offline reference. No device, ADB, save modification, Windows setting change or publication occurred in this stage.

## User report and scope

After the initial item turn-in fix the user could hand in Medusa to Sidorovich and the modified jacket to Nimble. New explicit report: both remain active in PDA after receiving rewards. Wolf's mission to clear boars across the road does not count after visible boars are killed. User is away until evening, so runtime/FPS actions wait. This separates a reported root-completion defect from the earlier possession/signature defect.

## Proven native defects and minimal correction

Only production file changed in this stage: src/xrGame/GameTask.cpp.

1. SScriptTaskHelper::init_functors used unconditional v_dest.resize(v_src.size()). The SoC XML loader CGameTask::Load directly resolves root complete/fail predicates and actions into runtime vectors, leaving the script-name helper lists empty. CGameTaskManager::GiveGameTaskToActor calls CommitScriptHelperContents on that root. An empty helper therefore cleared valid XML functions. The same occurs after the existing SoC Load(m_ID) in CGameTask::load. Lua can record rewarded while native root remains InProgress; CUITaskListWnd::UpdateList still lists it as active. The new guard returns only when ShadowOfChernobylMode && v_src.empty(). All four XML function families survive; nonempty script replacement and later-game behavior stay unchanged.

2. SGameTaskObjective::load restores serialized helper function names, but dynamically created Lua child objectives did not resolve those names back into runtime functors after a save reload. In the child loop of CGameTask::load, immediately after load_data(m_Objectives[i], stream), call m_Objectives[i].CommitScriptHelperContents() only for SoC. Root already had an explicit commit. This restores all four child predicate/action families, parent/index and ongoing/completed saved states. The empty-source guard also preserves XML child functions already supplied by Load(m_ID).

No serializer fields/order, game task data, target counts, inventory, rewards, cycle rules, Lua production scripts or save files were changed. Earlier SoC task_id/objective-index and root state callback fixes remain intact. CS/CoP use the original empty-list and load behavior; they are not device-validated.

## Already-rewarded saves and PDA

Original task_manager.script tracks reward_status=rewarded. With its XML root predicate restored, task_complete(task_id,0) can finish the native root on a subsequent game update. Original completion action and cyclic reset then execute. Offline regression explicitly saves and reloads original Lua CRandomTask plus actual native CGameTask while the item is absent and payment already occurred once. No reacquisition or second payment is introduced. This is a recovery expectation for the user's old Medusa/jacket saves, not a live-device proof. PDA rendering itself was inspected in source, not executed; no UI patch was needed.

## Wolf's boars: established boundary

For eliminate_lager, the shipped script requires the selected smart-terrain's oo.gulag:get_population_comed()==0. Visible boars dying nearby is not sufficient evidence that the selected group is empty. Child predicate restoration after reload can correct one real evaluation failure; the root fix can correct post-reward finalization. Actual task ID, game_tasks.xml/task_manager.ltx and base gulag scripts are not present locally. The user's mission was identified as Wolf/across the road, but its actual selected target/population is not known. Do not claim that all boar missions are fixed, change population rules or force-complete the user's task. If it persists on the new APK, inspect actual task ID/selected terrain/population and correct-group deaths before changing scripts.

## Validation and limits

evidence/questlife-tests contains an extract-and-run suite using ten actual production method bodies unchanged, original repository Lua 5.1 host implementation and unchanged task_manager.script. Explicit XML binding, typed stream, inventory/NPC/gulag/time/payment fixtures isolate lifecycle behavior. All15 fixed cases pass; removing exactly the two new additions reproduces13 relevant failures, with nonempty-overlay and later-game controls passing in both. Cases cover all four root/child families, failure actions, idempotence, item give/load/reward/cycle, already-rewarded Medusa/jacket load and Wolf selected population1 versus0 before/after load. The XML fixture matches the production direct-runtime-vector contract but does not parse real game XML. Typed streams exercise actual save/load methods, not actual binary save encoding. The full engine luabind runtime, live PDA and the user's active Wolf target are not executed.

Existing native dispatch/item Lua regressions were rerun against this new GameTask.cpp and pass. Actual Android Release syntax checks pass for GameTask.cpp and GameTask_script.cpp. Source hashes and exact compiler commands are preserved. No repeated broad performance tests are needed for the same two changes; the full APK compile/link is the next integration check.

Read-only action-return audit: src/xrScriptEngine/Functor.hpp returns TResult directly; actions still use legacy functor<bool> and can return nil. In this Android configuration XRAY_SCRIPTS_NO_BACKWARDS_COMPATIBILITY is undefined (zero mentions among2272 compile commands, including18PCH entries, and no source definition). luabind/detail/call_function.hpp only throws on mismatch if that macro is enabled; bool converter uses lua_toboolean, so nil becomes false and CallAllFuncs ignores it after executing the action. No action-vector type or global Lua conversion change is needed. Enabling that strict compatibility macro later requires a separate full script audit.

## Reproduce, build and continue

Restore evidence/questlife-tests to W/work/questlife-tests beside work/openxray-build and work/nightquest-tests. The earlier nightquest evidence includes lua_bridge.c; its include expects the same sibling layout. Use the documented bundled Python and MinGW paths, then run extract_and_run.py. Tests produce host executables locally, never commit/export them. New helper paths are work/questlife_pipeline.py and work/install_questlife.py, with portable copies in build-tools. These helpers must be copied back to W/work before use; they are not standalone inside docs.

From W: python work/questlife_pipeline.py prepare; .\work\build.ps1 -Variant questlife *> .\work\build-questlife.log; python work/questlife_pipeline.py verify. Prepare requires a clean committed tree and refuses overwriting an existing freeze. Verify requires all frozen tracked files/submodules/helper hashes unchanged; checks native package/signature/ELF/assets and writes a distinct APK/patch/symbols set. Keep installed nightpolish and daypolish reference APKs. After an actual successful build, record hashes in this report and PROJECT_STATUS/CONTINUE_PROMPT before sharing.

First runtime check when the user returns: guarded backup/update preserving certificate/UID/config/saves, load existing already-rewarded Medusa/jacket save, let task update and inspect active/completed PDA status without taking another quest or reacquiring items. Confirm no second reward. Inspect Wolf selected task/group and completion before/after reload; keep population rules unchanged. Then perform inherited daypolish panel/AA/config checks and a separate first-minute -physics_audit crate scenario. Do not run a live install or gameplay while the user is away.
