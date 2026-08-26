## Why

Authors have no in-editor diagnostic list. Engine `LOG_*` and Play-process C# output go to OS terminals (the editor even `AllocConsole`s a black window). Unity-style debugging — see `Debug.Log` / exceptions in a docked Console, filter, collapse, pause on Error — does not exist. Play is a separate process, so Player messages must be forwarded or they stay invisible.

## What Changes

- Ship the **Console**: docked diagnostic list (sibling tab to Content Browser), not a command prompt, not an in-game overlay, not History Panel
- Record **Console Messages** from the Editor Session and the Play Process (one list, **Console origin** tagged)
- Map `LOG_INFO/WARN/ERROR/FATAL` to Log / Warning / Error; `LOG_DEBUG` stays off the Console
- Add **Debug API** (`Debug.Log` / `LogWarning` / `LogError`) on `Blunder.Api`; `System.Console.WriteLine` is not a Console Message; no context Object / Ping
- Capture **Console stack** on Debug API (and Lifecycle exceptions); show it in **Console detail**; no jump-to-IDE
- Catch **Lifecycle exceptions** on engine-invoked C# (Ready, Tick, OnMessage, PoseApplied and same-class host callbacks): Error + stack, abort that invocation, continue siblings, do not kill the Player
- Toolbar: **Console collapse** (off by default), **Console clear**, **Clear on Play** (on by default), **Error Pause** (off by default; Play-origin Errors only), **Console filter** (three severity toggles + search)
- Ring **Console capacity** 10000 emits; **Console time** `HH:mm:ss`; oldest-at-top
- **Play log forwarding** on the existing **Play control channel** ([ADR 0040](../../../docs/adr/0040-console-play-log-on-control-channel.md))
- Product path does not `AllocConsole`; **Attached terminal** may still receive stdout including debug

**Out of scope:** command REPL; tilde overlay; double-click opens IDE; `Debug.Log(message, object)` Ping; stdout capture as the feed; a second log socket; Fatal as a fourth Console severity; merging editor/Player origins under Collapse

## Capabilities

### New Capabilities
- `console-panel`: Editor Console dock UI, message ring, severity map, collapse/filter/clear/capacity/time, no product AllocConsole
- `debug-api`: C# Debug API, native log C-ABI, Console stack, Lifecycle exception → Error Console Message

### Modified Capabilities
- `play-mode`: Clear on Play; Error Pause → Play Pause; editor ingests forwarded Player Console Messages
- `play-player`: emit Console Messages on the Play control channel; survive Lifecycle exceptions; do not AllocConsole
- `object-message`: a Lifecycle exception in one `OnMessage` does not stop later receivers in the snapshot
- `engine-c-abi`: log entry point; ABI version >= 11
- `script-native-abi`: NativeAbi completeness includes the log pointer

## Impact

- `LogSystem`: ring + Console sink; stop `AllocConsole`; keep stdout if an attached terminal exists
- Slint: replace `DockPanelKind::custom` Console placeholder with a real panel (`DockPanelKind::console` or equivalent); docked + floating
- Play IPC: bidirectional — Player → editor log records on the same loopback connection as pause/resume/stop
- `Blunder.Api` / ScriptHost / `engine_c_abi`: Debug + exception catch on host-invoked C#
- Docs: CONTEXT Console terms (grilled); ADR 0040
- Tests: severity map, ring drop, collapse key, IPC log round-trip, exception continues siblings, ABI completeness
