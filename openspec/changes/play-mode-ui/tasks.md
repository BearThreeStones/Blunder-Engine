## 1. Player executable skeleton

- [x] 1.1 Add `engine_player` CMake target (thin main, link `engine_runtime` / needed deps; stage beside `engine_editor`)
- [x] 1.2 Parse CLI: `--project-root`, `--scene`, `--play-ipc` (endpoint)
- [x] 1.3 Boot Player without editor Slint shell: window + engine loop + FileSystem project root
- [x] 1.4 Load Play entry scene asset and confirm manual run from command line

## 2. Player host + Pause

- [x] 2.1 Start DotNetHost in Player (process NativeAbi); load `.blunder/scripts_bin` game DLL when present
- [x] 2.2 Mount scene Behaviours after load when host + assembly ready
- [x] 2.3 Implement Pause flag: skip Behaviour Tick while rendering/orbit still work; Resume clears flag
- [x] 2.4 Window close exits process cleanly

## 3. Play control channel

- [x] 3.1 Shared IPC helper (localhost TCP ephemeral port default): listen in Player, connect from editor
- [x] 3.2 Commands: `pause`, `resume`, `stop`; Player emits `ready` when accepting commands
- [x] 3.3 Unit/smoke test: fake client pause/resume/stop against Player or in-process IPC loopback

## 4. Editor Play session

- [ ] 4.1 `PlaySessionController` (or equivalent): states Stopped / Starting / Playing / Paused; track child process
- [ ] 4.2 Spawn sibling `engine_player` with project root, active scene saved path/GUID, IPC endpoint
- [ ] 4.3 Single-session rule: Stop existing before new Play; process exit → Stopped
- [ ] 4.4 Wire Play / Pause / Stop to Slint (replace stub); Pause enabled only after `ready`

## 5. Preflight gates

- [ ] 5.1 Dirty scene prompt: Save and Play / Play last saved / Cancel
- [ ] 5.2 Scripts dirty detection + `ScriptsBuilder`/`dotnet build` before spawn; fail → stay Stopped
- [ ] 5.3 Tests for dirty prompt decision paths and scripts-dirty build gate (fakes OK)

## 6. Edit Mode host policy + docs

- [ ] 6.1 Stop product auto-start of DotNetHost from editor `startSystems` for Play; keep `BLUNDER_DOTNET_SCRIPTS` as debug opt-in only
- [ ] 6.2 Update `docs/agents/testing.md` and build docs for `engine_player` + Play session
- [ ] 6.3 Manual checklist: Play → Pause → Resume → Stop; close window; dirty prompt; Scripts dirty build

## 7. Verify

- [ ] 7.1 Automated tests for session controller + IPC green
- [ ] 7.2 Smoke: editor Play mounts fixture Behaviour Tick in Player (or documented manual if UI-only)
