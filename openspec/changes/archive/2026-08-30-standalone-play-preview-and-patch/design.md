## Context

Product Play is already one `engine_player` process on a loopback TCP control channel ([ADR 0014](../../../docs/adr/0014-play-mode-separate-player-process.md)). Editor `PlaySessionController` spawns that process, waits for `ready`, and sends line verbs (`pause`, `resume`, `stop`, `step N`, `frame`). The Player parses those in `handlePlayIpcCommand` and replies with NDJSON (`kind: log|frame|error`).

Missing vs this change: no Reload/patch/pose verbs; Application Bar is Play/Pause/Stop only (`editor_window.slint`); `startPlaySession` always reads `activeScenePath()` (so Reload would retarget if we reused it); `SceneSystem::loadScene` returns a cached instance and `AssetManager::loadScene` can serve a stale `SceneAsset` after editor Save; `DocumentHistory::push` / undo / redo / jump have no Play observer.

See `proposal.md` for why. Specs under `specs/` are the behavior contract.

## Goals / Non-Goals

**Goals:**
- Extend the existing Play control channel (one socket) with Reload, v1 authorship patch, and pose preview
- Capture Play entry (path + Scene Asset GUID) at spawn and reuse it for Reload and patch gating
- Reload: force disk re-read + instantiate-then-swap so a failed load keeps the current world
- Patch: observe sealed History on the Play entry Live document; address by entity name
- Pose preview: editor viewport overlay of Player local TRS; Live / Inspector / gizmo stay authored

**Non-Goals:**
- A second Play socket, PIE, in-place Tick, or dumping `SceneInstance` across processes
- ALC / Player Asset hot-reload / Spawn-delete-rename-reparent patches
- Writing preview poses into Document History or treating Play frame stills as the scene view
- Changing Play-while-Playing (still Stop then spawn)

## Decisions

1. **Same channel, additive verbs (not a new protocol)**  
   Keep editor→Player as newline verbs. Add `reload` (no payload: Player already has `--scene` from spawn). Add `patch <json>` (one JSON object after the prefix, same pattern as `step N`). Player→editor stays NDJSON with new `kind` values: `issue`, `reload`, `poses`.  
   *Rejected:* a second socket; replacing all host commands with JSON in this slice; sending a new scene path on Reload (entry is frozen).

2. **Play entry captured on the session, not reread from the open editor scene**  
   On successful `play()`, `PlaySessionController` stores the request scene path and the Scene Asset GUID from the editor registry. Reload, patch gating, and pose identity use that capture. `startPlaySession` stays the spawn path only.  
   *Rejected:* Reload calling `startPlaySession` / `activeScenePath()`; retargeting when the author opens another scene.

3. **Reload is instantiate-then-swap on the Player**  
   On `reload`: `AssetManager::invalidateSceneCache(entry)` → `loadScene` into a **new** `SceneInstance` (do not return the cached loaded instance) → if instantiate fails, drop the new instance and ack `{"v":1,"kind":"reload","ok":false}` → if ok, `setActiveInstance(new)`, `unloadSceneInstance(old)`, remount is already inside `instantiateScene` via existing `mountSceneBehaviours`, ack `ok:true`. Do not unload the current world first. Do not `dotnet build` or reload the game assembly. Pause flag is untouched; Ready still runs because mount runs.  
   *Rejected:* unload-first (failure would leave an empty world); `loadScene` early-return of the existing instance; treating Reload as Stop then Play.

4. **Editor Reload preflight reuses Play gates, not spawn**  
   Windowed: if the Live document **with the captured Play entry GUID** is dirty, reuse `decidePlayDirtyScene` and `PlayDirtySceneDialog` with Reload copy (`Save and Reload` / `Reload Last Saved` / Cancel). Headless: last saved, no modal. Then invalidate/load the captured On-disk asset, `runPlayCameraGate`, send `reload` only if the camera gate is ok. Scripts dirty → Warning Issue `scripts.dirty`, still send `reload` (no `runPlayScriptsGate`). Clear on Play is not invoked. Failure (gate or `reload ok:false`) leaves session Playing/Paused.  
   *Rejected:* prompting to save a different open scene; building Scripts on Reload; Console clear on Reload; killing the process on failure.

5. **Patch is a post-mutation v1 snapshot, not a Command opcode**  
   After `DocumentHistory::push`, `undo`, `redo`, or `jumpTo` on the Play entry Live document: if the session is Playing or Paused and the mutated Command is in the v1 catalog, serialize the **current** authored v1 fields for that entity’s Authorship Address (name) and send `patch {…}`. Undo/Redo/Jump then naturally send restored values. Spawn/delete/rename/reparent/Global Commands: no message. Uncommitted gizmo samples: no message (seal only).  
   v1 JSON (all keys optional except `address`): `address`, `local` `{t,r,s}`, `unique` (attachment type names present on the entity), `mesh_renderer`, `behaviours` (type + id + property bag as in scene JSON), `skeleton_modifiers`, `animation_player` / `animation_tree` host fields already on the scene entity. Player `findEntityByName`; missing name → skip write, emit `{"v":1,"kind":"issue","sev":"warning","code":"play.patch_unknown_address","address":"…"}`. Editor ingests as Warning Issue. That is not `PlayIpcErrorRecord`, not Console Error, not Error Pause, not History rollback.  
   *Rejected:* streaming pointer samples; opcode-only patches that the Player must invert; EntityId/ObjectId keys; Pause-only apply.

6. **Pose preview is an editor-viewport overlay map, not a second SceneInstance**  
   Each Player `SDL_AppIterate` (after Tick or while paused/render), send one `{"v":1,"kind":"poses","entities":[{"name","t","r","s"}, …]}` for entities that have a scene name (Authorship Address). Local TRS from the Play Process entity. Editor holds `address → local TRS` and applies it only when gathering editor-viewport mesh/world matrices. Inspector, Transform gizmo handles, and Live `SceneInstance` stay authored. Stop or Reload clears the map (Reload fills again from the new world). Runtime-spawned unnamed entities omitted. Headless Player still emits; Headless Editor has nothing to draw.  
   *Rejected:* writing poses into Live; cloning a PIE world in the editor; using Play frame RGBA as the scene view; a second socket.

7. **Ship order matches ADR 0049**  
   Implement and test Reload (IPC + Player swap + editor control + dirty/camera) before patch (History hook + Player apply) before pose preview (emit + viewport overlay). Shared IPC parsing can land with Reload so later slices only add `patch` / `poses`.

## Risks / Trade-offs

- **[Stale SceneAsset after Save]** → Player always `invalidateSceneCache` before Reload load; editor camera gate does the same on the captured path after Save and Reload.
- **[loadScene returns the live instance]** → Reload path must not use the “already loaded” early return; instantiate a new instance, then swap.
- **[Patch during Tick races]** → Accepted: Tick may overwrite; no authorship lock (spec). Apply on the Player main thread in `handlePlayIpcCommand` / next iterate before Tick, not from a second thread.
- **[Pose NDJSON size]** → One line per iterate; reuse `truncatePlayIpcField` / cap entity count if needed; names + 9 floats, not meshes.
- **[History hook misses a Command type]** → Start from known v1 command classes (`SetEntityTransformCommand`, Unique/MeshRenderer/Behaviour/SkeletonModifier/animation-host setters). Unknown types: no patch (editor-only), not an Issue.
- **[Reload while Starting]** → Ignore Reload until `ready` and Playing/Paused, same as Pause.

## Migration Plan

No on-disk format change. Older Players that ignore unknown host lines stay Pause/Stop compatible; new verbs no-op until this Player ships with the editor. Rollback: hide the Reload control and leave `patch`/`poses` unsent.

## Open Questions

None that change specs. Pose send rate stays “once per Player iterate”; throttle only if profiling shows the channel saturates.
