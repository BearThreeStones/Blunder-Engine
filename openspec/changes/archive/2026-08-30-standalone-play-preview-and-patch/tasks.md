## 1. Play control channel verbs

- [x] 1.1 Extend `PlayIpcCommand` / `PlayIpcHostCommand` with `Reload` and `Patch`; parse `reload` and `patch <json>`; keep unknown lines ignored
- [x] 1.2 Add Player→editor NDJSON kinds `issue`, `reload` (`ok` bool), and `poses` (name + local TRS array); poll them on `PlayIpcServer`
- [x] 1.3 Add `k_issue_play_patch_unknown_address` (`play.patch_unknown_address`) next to existing Issue codes
- [x] 1.4 Extend `play_ipc_test` for parse/format of reload, patch, issue, poses; one-socket still (no second listen)

## 2. Player Play Reload

- [x] 2.1 Add a Player Reload path: `invalidateSceneCache` + instantiate a **new** instance (no `loadScene` already-loaded early return) + swap active + unload old; on instantiate failure keep the current world
- [x] 2.2 Handle `reload` in `handlePlayIpcCommand`: do not change pause flag; remount from the already-loaded assembly; ack `kind:reload`; do not `dotnet build`
- [x] 2.3 Cover Reload keep-world-on-failure and successful swap with a first-party test (hooks or in-process SceneSystem; do not require spawning `engine_editor`)

## 3. Editor Play Reload session

- [x] 3.1 Capture Play entry path + Scene Asset GUID on successful `play()`; Reload/patch/pose identity use that capture, not `activeScenePath()`
- [x] 3.2 Add `PlaySessionController::reload()`: Playing or Paused + ready only; skip Scripts build; Warning `scripts.dirty` does not block; camera gate on captured On-disk asset after dirty-prompt save rules; send `reload`; `ok:false` or missing camera leaves Playing/Paused
- [x] 3.3 Dirty prompt for Reload only when the Play entry Live document (same GUID) is dirty; Headless last-saved; dialog copy Save and Reload / Reload Last Saved / Cancel; Save does not call Reload; Play-while-Playing stays Stop then spawn
- [x] 3.4 Application Bar Reload control; `UiEventKind` + `UiHost` path; Clear on Play must not run on Reload
- [x] 3.5 Extend `play_session_controller_test` and `play_preflight_test` for capture, no-spawn Reload, dirty/headless, camera-abort, Scripts-dirty-warn, no Console-clear

## 4. Play authorship patch

- [x] 4.1 After Document History `push` / undo / redo / `jumpTo` on the Play entry Live document, if the Command is v1-patchable, send a `patch` snapshot (address + current v1 fields); other documents and Spawn/delete/rename/reparent/Global: no message
- [x] 4.2 Player applies v1 snapshot by `findEntityByName` while Playing or Paused; unknown address skips write and emits Warning `kind:issue` `play.patch_unknown_address` (not `error`, not Error Pause, not History rollback)
- [x] 4.3 Tests: seal Transform patches; Undo restores; unknown address Warning; non-entry document silent; uncommitted gizmo does not patch

## 5. Play pose preview

- [x] 5.1 Player emits `poses` once per iterate for named Play-entry entities (omit unnamed runtime spawns); same control channel
- [x] 5.2 Editor overlay map `address → local TRS` applied only to editor-viewport mesh/world gather; Inspector and gizmo handles stay Live; Stop and Reload clear the map (Reload fills from the new world)
- [x] 5.3 Tests: parse/ingest poses; overlay does not dirty History; unnamed entity omitted; Stop clears overlay

## 6. Docs and validation

- [x] 6.1 Point `docs/agents/testing.md` Play session table at Reload / patch / pose tests; no CONTEXT/ADR rewrite unless a task uncovers a mismatch
- [x] 6.2 Run `play_ipc_test`, `play_session_controller_test`, `play_preflight_test`, and any new Reload/patch/pose targets with `ctest --output-on-failure`
