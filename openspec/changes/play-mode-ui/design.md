## Context

ADR 0014 / CONTEXT **Play** section: Play Mode is a separate **Player** process (`engine_player`), not in-editor simulation. Today DotNetHost starts in the editor only via `BLUNDER_DOTNET_SCRIPTS`; `editor_window.slint` has an unwired Play button. Behaviour scene mount already works when a host is running in-process. Project Manager already spawns sibling `engine_editor` (`project_relaunch`) — Play can mirror that pattern for `engine_player`.

## Goals / Non-Goals

**Goals:**
- Editor Play / Pause / Stop drive one Play session (spawn Player, IPC, track exit)
- Player loads saved entry scene + Scripts, runs DotNetHost + mount + Tick; Pause skips Tick
- Dirty scene prompt; Scripts build when dirty before spawn
- Demote product reliance on editor-process host for Play

**Non-Goals:**
- Live sync / ALC / asset hot-reload into running Player
- Multi-Player; shipping storefront game packaging
- Splitting `engine_runtime` into editor-free `engine_core` (Player may link fat runtime but must not start editor chrome)
- Pause that freezes rendering as the primary definition
- Full Behaviour Inspector

## Decisions

1. **Player binary = thin main + shared runtime (P1+P3)**  
   New `engine_player` exe beside `engine_editor` / `project_manager`. First slice may link current `engine_runtime` but startup path skips Slint editor shell / dock — Game window + engine loop + host only.  
   *Rejected:* `engine_editor --play` as long-term Player; fully forked engine tree.

2. **CLI contract**  
   Player takes at least `--project-root <path>` and `--scene <virtual-or-guid-or-path>` (Play entry scene = editor active scene’s saved asset). Optional `--play-ipc <endpoint>` for the control channel.  
   *Rejected:* Memory-dumping the live SceneInstance across processes.

3. **IPC = localhost TCP or named pipe, JSON line commands**  
   Minimal verbs: `pause`, `resume`, `stop`; Player may emit `ready` once listening. Prefer a small shared C++ helper used by editor session controller and Player. Windows: named pipe OR `127.0.0.1` port passed on CLI — pick one in implementation; default **localhost TCP with ephemeral port** for easier tests.  
   *Rejected:* stdin-only; Stop-as-kill without pause channel; internet-facing RPC.

4. **Pause = skip Behaviour Tick (Z1)**  
   Player holds `paused` flag; frame loop still renders and accepts camera orbit; `LifecycleDispatch::invokeTick` (and equivalent gameplay sim time) skipped while paused.  
   *Rejected:* Tear down host on Pause; freeze GPU present as the only Pause.

5. **Session controller in editor**  
   Own spawn, IPC client, process wait, UI state machine (Stopped / Starting / Playing / Paused). Concurrent Play request Stops first. Process exit or `stop` ack → Stopped.  
   *Rejected:* Fire-and-forget spawn with no tracking.

6. **Scripts dirty build (C2)**  
   Compare Scripts sources / csproj inputs to `.blunder/scripts_bin` outputs (or record last-build stamp); run existing `ScriptsBuilder` (or `dotnet build`) only when dirty; failure aborts Play.  
   *Rejected:* Build every Play; Player runs `dotnet build`.

7. **Edit Mode host policy (E1)**  
   Remove product path that auto-starts DotNetHost from editor `startSystems` for Play. Keep `BLUNDER_DOTNET_SCRIPTS` as explicit debug opt-in. Player always starts host for the session (non-fatal log if Scripts missing — scene still loads with null peers per behaviour-scene).  
   *Rejected:* Requiring editor host for Play.

8. **Dirty scene prompt (D2)**  
   If DocumentHistory / scene dirty: modal Save and Play / Play without saving / Cancel. Save uses existing export path.  
   *Rejected:* Silent auto-save; silent old disk.

## Risks / Trade-offs

- **[Fat runtime in Player]** → Accept first; document “no editor chrome”; split libs later.
- **[IPC flaky / port conflict]** → Bind ephemeral; pass port on CLI; timeout Starting state.
- **[Scripts dirty heuristic wrong]** → Prefer stamp file or `dotnet build --no-restore` when in doubt; log why build ran.
- **[Double host if debug env set during Play]** → Document: avoid `BLUNDER_DOTNET_SCRIPTS` while using Player Play; optional warn.
- **[Pause without IPC up yet]** → Disable Pause until `ready`; Starting state.

## Migration Plan

1. `engine_player` stub + CLI + scene load + host (manual run).
2. IPC pause/resume/stop + unit/smoke tests.
3. Editor session controller + Play/Pause/Stop UI + dirty/Scripts gates.
4. Demote editor auto-host; update testing.md.
5. Rollback: keep env host; leave Player unused.

## Open Questions

- Exact IPC transport (TCP vs named pipe) — **default TCP localhost** unless Windows policy pushes pipes.
- Whether Player uses Slint for an empty chrome window vs raw SDL/Vulkan window — **prefer minimal existing window path already used by engine without full editor_window**.
