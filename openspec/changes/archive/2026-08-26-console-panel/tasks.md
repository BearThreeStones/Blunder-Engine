## 1. Console ring and LogSystem

- [x] 1.1 Add a mutex-protected Console ring (cap 10000, drop oldest) next to `LogSystem`, with severity Log/Warning/Error, text, origin, time, optional stack
- [x] 1.2 Map spdlog info/warn/error/critical into the ring; skip debug; record Error before existing `LOG_FATAL` throw
- [x] 1.3 Stop `AllocConsole`; attach stdout color sink only when a console is already present
- [x] 1.4 Tests: debug does not append; info→Log; cap drops oldest; clear empties both origins

## 2. C-ABI log and Debug API

- [x] 2.1 Add `blunder_log` (severity, UTF-8 text, optional stack); bump `BLUNDER_ENGINE_C_ABI_VERSION` to 11; fill NativeAbi process/module tables
- [x] 2.2 Extend `BlunderNativeAbi` / completeness checks; update `native_abi_test` and related ABI version asserts to >= 11
- [x] 2.3 Add `Blunder.Api.Debug.Log` / `LogWarning` / `LogError` (string + stack, no context Object) through the registered log pointer
- [x] 2.4 Update `Blunder.Api.NativeAbiTests` stub size/completeness for the log entry

## 3. Lifecycle exception catch

- [x] 3.1 try/catch `OnTick`, `OnReady`, `OnMessage` in ScriptHost; log Error + stack; do not rethrow
- [x] 3.2 try/catch PoseApplied managed native thunk the same way
- [x] 3.3 Test: two Behaviours, first Tick throws → Error row, second still Ticks, process lives
- [x] 3.4 Test: first `OnMessage` throws → second receiver still gets the Message

## 4. Play log forwarding

- [x] 4.1 NDJSON log records on the existing Play IPC after `ready`; keep `pause`/`resume`/`stop`/`ready` as bare lines; cap text/stack ~16KiB
- [x] 4.2 `PlayIpcServer` poll after handshake (do not drop post-ready logs); ignore non-ready lines until `ready`
- [x] 4.3 Player frame IPC poll flushes the local ring to the editor; editor ingest tags Play Process origin
- [x] 4.4 Tests: log after `ready` is ingested; log before `ready` is not treated as a Console Message; existing pause/resume/stop tests still pass

## 5. Play session Console hooks

- [x] 5.1 Clear on Play (default on) clears the editor ring when a session starts after successful preflight
- [x] 5.2 Error Pause (default off): Play-origin Error while Playing issues the existing Pause command; editor-origin Error does not
- [x] 5.3 Stop does not clear the ring
- [x] 5.4 Tests for Clear on Play on/off and Error Pause origin rules (session/IPC fakes OK)

## 6. Console panel UI

- [x] 6.1 Add `DockPanelKind::console` as 6 (after `animation`); seed default layout with it instead of `custom`; auto-hide edge bottom
- [x] 6.2 `console_panel.slint`: list (oldest top, `HH:mm:ss`), detail (text + stack), Collapse / Clear / Clear on Play / Error Pause / three filters + search
- [x] 6.3 Sync ring → visible rows (filter then collapse); counts are pre-collapse; docked + floating hosts
- [x] 6.4 Replace placeholder `panel-kind == 0` Console content in `editor_window.slint` / `floating_panel_window.slint`

## 7. Validation

- [x] 7.1 Confirm CONTEXT Console terms and ADR 0040 match the shipped behavior
- [x] 7.2 Build `vs2026-debug` and run `play_ipc`, `native_abi`, and new console/lifecycle tests
- [x] 7.3 Manual: Console tab beside Content Browser; `Debug.Log` from Play; exception Error; Collapse/filter/Clear on Play/Error Pause; no AllocConsole
