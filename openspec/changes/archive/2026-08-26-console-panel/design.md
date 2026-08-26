## Context

See proposal.md for motivation. Grilling locked CONTEXT Console terms; [ADR 0040](../../../docs/adr/0040-console-play-log-on-control-channel.md) locks forwarding onto the Play control channel.

Today: `LogSystem` uses an async spdlog logger with stdout only; Win32 `AllocConsole` for `WIN32_EXECUTABLE`. `LOG_*` never reaches Slint. Default dock seeds a **Console** widget as `DockPanelKind::custom` (placeholder `"Panel"`). Content Browser is kind `4`; custom is `0`. History lives *inside* Content Browser (`FilesystemHistoryHost`), not as that Console widget.

Play IPC is a loopback TCP line protocol: editor listens, Player connects, sends `ready`, then receives `pause` / `resume` / `stop`. `PlayIpcServer::tryReadReady` currently ignores non-`ready` lines and does not read after handshake. C# `OnTick` / `OnReady` / `OnMessage` / PoseApplied have no catch. `BlunderNativeAbi` is v10 with no log pointer.

## Goals / Non-Goals

**Goals:**
- One in-process Console ring that both `LOG_*` (mapped) and the C-ABI log entry write
- Slint Console panel on a new `DockPanelKind::console` (append after `content_browser`, do not renumber existing kinds)
- Bidirectional Play IPC: keep text commands; add NDJSON log records Player → editor
- ScriptHost try/catch on every engine-invoked C# entry used in product Play
- Stop `AllocConsole`; keep stdout sink only when a console is already attached

**Non-Goals:**
- Persisting Console toolbar toggles to disk
- Jump-to-IDE, context Object, command REPL
- Byte-accurate memory budget (count cap only)
- Changing History Panel placement

## Decisions

### D1 — Console ring beside LogSystem, not a second logger
**Choice:** A mutex-protected ring (cap 10000) owned next to `LogSystem`. An spdlog callback/sink (or equivalent) appends mapped info/warn/error/critical. `LOG_FATAL` still throws after the Error row is stored. `debug` does not append. C-ABI `blunder_log` appends the same ring (Player origin implied by host mode).
**Why:** One list identity; UI and IPC drain the same structure.
**Rejected:** UI parsing stdout; a Slint-only buffer that C# cannot share with C++.

### D2 — Append `DockPanelKind::console` as 6
**Choice:** `custom=0 … animation=5`, new `console=6`. Replace `seedDockingWorkspace` custom Console widget with this kind. Slint `if panel-kind == 6` hosts `ConsolePanel` in `editor_window.slint` and `floating_panel_window.slint`. Auto-hide default edge: bottom (same as Content Browser).
**Why:** Existing kind integers are wired in Slint; inserting in the middle would swap Hierarchy/Inspector/Animation. Animation already occupies 5.
**Rejected:** Reusing `custom`; nesting Console in `FilesystemHistoryHost`; stealing kind 5.

### D3 — NDJSON log records on the existing socket
**Choice:** After `ready`, Player may write one JSON object per line, e.g. `{"v":1,"sev":"log|warning|error","text":"...","stack":"...","ms":<unix_ms>}`. Commands stay bare `pause`/`resume`/`stop`/`ready`. Cap `stack` (and `text` if needed) before send (~16KiB) so one line cannot stall the socket. Editor polls incoming lines each editor frame after ready (extend `PlayIpcServer`; do not drop unknown pre-ready lines silently forever — ignore them until `ready`, then parse JSON).
**Why:** Stacks contain newlines; JSON avoids a custom escape scheme. Bare commands keep current Play IPC tests working.
**Rejected:** Second socket; stdout scrape; binary frames.

### D4 — Drain Player ring on the Player frame that already polls IPC
**Choice:** Player `poll()` of `PlayIpcClient` stays on the game/editor tick path; after handling host commands, flush pending ring entries as NDJSON. Editor ingest applies origin=Player, then Error Pause / UI sync.
**Why:** Same thread story as pause/resume; no extra log thread.
**Rejected:** Sending from the spdlog async worker (reentrancy / ordering vs Tick).

### D5 — NativeAbi v11 log pointer
**Choice:** Add `log` to `BlunderNativeAbi` / `engine_c_abi.h` (`severity`, `text`, `stack` UTF-8, stack may be null). Bump `BLUNDER_ENGINE_C_ABI_VERSION` to 11. `Debug.Log*` captures `Environment.StackTrace` (or `new StackTrace(skip)`) then calls the pointer. Completeness checks require non-null.
**Why:** Same registration path as Message/Input; Player and env-gated editor host share one call.
**Rejected:** P/Invoke a new DLL export that bypasses the table.

### D6 — Catch at ScriptHost unmanaged entry points
**Choice:** try/catch in `OnTick`, `OnReady`, `OnMessage`, and PoseApplied's managed native thunk. On catch: `Debug.LogError` (or C-ABI log Error) with exception message + stack, then return. Do not rethrow.
**Why:** These are the `UnmanagedCallersOnly` boundaries; catching inside each Behaviour method is not enforceable.
**Rejected:** Catch only Tick; AppDomain unhandled hook as the only path.

### D7 — Collapse and filter are view, ring is source of truth
**Choice:** Ring stores every emit. Slint model is rebuilt on sync: apply search + severity toggles, then optional collapse. Counts on toggles read the ring, not the visible rows. Collapse key hash: text + severity + stack + origin.
**Why:** Toggling Collapse must not destroy history; Clear is the only drop besides the cap.
**Rejected:** Collapsing inside the ring when the toggle is on.

### D8 — Session-only toolbar state
**Choice:** Collapse, Clear on Play, Error Pause, severity toggles, search string live on the Slint window (like History filters). Defaults: Collapse off, Clear on Play on, Error Pause off, all severities on, search empty. Not written to the Project.
**Why:** Matches other editor chrome; no new settings file.
**Rejected:** Persisting in Project File for this slice.

### D9 — Attached terminal detection
**Choice:** Remove `AllocConsole`. Keep the stdout color sink only when a console is already attached (existing `GetConsoleWindow` / valid `GetConsoleMode` on stdout). Otherwise Console panel + ring only.
**Why:** Product surface is the panel; VS/cmd still see debug.
**Rejected:** Always AllocConsole; file log as a required v1 sink.

## Risks / Trade-offs

- **[Risk] IPC log volume stalls pause/stop → Mitigation:** cap line size; drop-oldest ring; send from frame poll not per-log on the spdlog thread; if send would block, drop newest Player forwards after a small pending queue (document: editor ring never exceeds 10000).
- **[Risk] tryReadReady currently discards non-ready lines → Mitigation:** after `ready`, a dedicated `pollLogs` must consume leftover buffer + new recv; tests for log-before-ready ignored, log-after-ready ingested.
- **[Risk] Slint model rebuild at 10000 rows → Mitigation:** rebuild on dirty, not every engine frame if the ring is unchanged; collapse reduces visible rows.
- **[Risk] ABI v11 breaks stale completeness tests → Mitigation:** update NativeAbi struct, fill table, native_abi tests, managed completeness in the same change.
- **[Risk] Exception catch hides native SEH → Mitigation:** catch `System.Exception` only on the managed side; native C++ `LOG_FATAL` still throws.

## Migration Plan

1. Ring + severity map + stop AllocConsole; unit tests
2. C-ABI log + NativeAbi v11 + Debug API
3. ScriptHost catches + object-message fan-out test
4. Play IPC NDJSON + editor ingest + Error Pause / Clear on Play
5. Slint Console panel + dock kind 6; replace placeholder
6. Rollback: hide panel / leave placeholder; no asset format change

## Open Questions

None that change specs. Exact JSON field names can be chosen at implement (`sev` vs `severity`) as long as tests lock one schema.
