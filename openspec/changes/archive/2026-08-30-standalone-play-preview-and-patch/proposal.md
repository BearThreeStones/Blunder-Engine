## Why

Play Mode already uses a separate Player process, but authors cannot pick up a saved scene without killing that process, cannot push sealed Live edits into the running world, and cannot see Tick-driven motion in the editor viewport. They asked for one Standalone Play path with bridges—not PIE and not Unity in-place Tick. The product contract is locked in CONTEXT and [ADR 0047](../../../docs/adr/0047-play-reload.md) / [0048](../../../docs/adr/0048-play-authorship-patch.md) / [0049](../../../docs/adr/0049-standalone-play-preview-and-patch.md).

## What Changes

- Add **Play Reload**: dedicated Play control; same Play Process reinstantiates the session’s captured On-disk **Play entry scene**; Save does not Reload; Play-while-Playing stays Stop then spawn
- Reuse **Play dirty prompt** for Reload (windowed three-way; Headless last-saved); re-run **Play camera preflight**; failure leaves the current Player world; no Scripts build/ALC; Scripts dirty is Warning not block; Ready on remount; Pause flag unchanged; Console not cleared
- Add **Play authorship patch**: sealed Document History Commands on the Play entry Live document apply to the Player by **Authorship Address**; v1 existing-entity authored data only; Tick may overwrite; Undo/Redo/Jump also patch; unknown name → Warning Issue `play.patch_unknown_address`; no History rollback
- Add **Play pose preview**: Player streams poses on the existing Play control channel; editor viewport draws them; Live / Inspector / gizmo handles stay authored; not Capture, not Play frame, not Query
- Reject as product Play: in-process PIE, Unity in-place Tick, dumping Live `SceneInstance`, replaying Live Commands after Reload, a second Play socket

**Out of scope:** ALC; Player Asset hot-reload; Spawn/delete/rename/reparent patches; writing Play poses into Document History; Game View dock of Play frames; Keep Simulation Changes

## Capabilities

### New Capabilities
- `play-authorship-patch`: sealed Live Editor Commands onto the running Play Process world (v1 catalog, address, Tick overwrite, History, unknown address)
- `play-pose-preview`: editor viewport observation of Play Process poses without mutating Live

### Modified Capabilities
- `play-mode`: Play Reload control and session rules; dirty prompt covers Reload; Edit during Play allows patches and pose preview; Play entry frozen at spawn; no product PIE
- `play-player`: handle Reload, apply patches, emit pose preview; remount from the already-loaded Scripts assembly
- `console-panel`: Clear on Play does not run on Play Reload

## Impact

- Editor: `PlaySessionController`, `UiHost` / Slint Application Bar Play cluster, `play_dirty_scene_dialog.slint`, Document History seal/undo hooks, editor viewport draw of preview poses
- Player: `engine/src/player/src/main.cpp` IPC handler; `SceneSystem::unloadSceneInstance` / `loadScene` / `instantiateScene` / `mountSceneBehaviours` for Reload
- IPC: extend `play_ipc` NDJSON on the existing loopback channel (reload, patch, pose records)
- Tests: `play_session_controller_test`, `play_ipc_test`, `play_preflight_test`; new Reload / patch / pose round-trips
- Docs: CONTEXT already updated; ADRs 0014/0047/0048/0049
