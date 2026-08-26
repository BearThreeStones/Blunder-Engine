## Context

See proposal.md. Grilling locked CONTEXT and [ADR 0044](../../../docs/adr/0044-machine-adapters.md). Authorship System, `captureScene`, `PlaySessionController` (including `waitForPlayFrame`), and Headless boot already exist. `editor_launch` today parses `--project-root` / `--headless` and Debug compiled-root fallback. Windowed/Headless Editor still opens a startup scene (`BLUNDER_STARTUP_SCENE` / default). `EditorSceneEditSystem::saveActiveScene` is the persist path. Thumbnail cache already encodes PNG via `stb_image_write`.

## Goals / Non-Goals

**Goals:**
- Parse adapter launch (`--mcp`, `--scene`, one CLI verb) on `engine_editor`; imply Headless; require `--project-root` for adapters
- Shared in-process dispatch for the verb set; CLI and MCP are presentations
- MCP JSON-RPC over stdio (initialize, tools/list, tools/call) without a new JSON library
- CLI one-shot: JSON stdout, PNG `--out`, then `requestQuit`
- Open Live only when `--scene` is set (do not use startup-scene default for adapters)

**Non-Goals:**
- HTTP MCP, attach, `engine_agent`, Play dump, CLI REPL
- New Authorship Ops
- Full MCP spec surface (resources, prompts, sampling)
- Windowed Editor hosting adapters

## Decisions

### D1 — Dispatch in engine_runtime, presentations in editor
**Choice:** A testable `MachineAdapter` / launch parse in `engine_runtime` (next to `editor_launch`). `engine_editor` main selects: windowed tick, bare Headless tick, MCP stdio loop, or CLI one-shot then quit. Player executable ignores adapter flags.
**Why:** Same kernel tests as Authorship; no third binary.
**Rejected:** Sidecar MCP server; putting JSON-RPC in Player.

### D2 — Closed JSON codec, no new dependency
**Choice:** First-party writer/parser for CLI result JSON and the MCP JSON-RPC subset. PNG via existing `stb_image_write` (same as thumbnail cache). MCP ImageContent is base64 PNG.
**Why:** engine_runtime already has yaml-cpp, not a JSON package we own. Closed schema.
**Rejected:** New nlohmann/rapidjson dependency; stdout raw PNG.

### D3 — `--scene` opens Live; adapters skip startup scene
**Choice:** Adapter launches do not call the GUI startup-scene path. `--scene` is `EditorSceneEditSystem::openScene`. Missing `--scene` leaves no Live document.
**Why:** Grilling: machines do not guess last-opened / default scene.
**Rejected:** `BLUNDER_STARTUP_SCENE` for MCP/CLI; last-opened prefs.

### D4 — CLI play-frame composes PlaySessionController
**Choice:** CLI `play-frame` calls existing `play` (Headless spawn, last-saved dirty rule), `waitForPlayFrame` (steps 0) or `pause` + `step(N)` + `waitForPlayFrame`, then `stop`. MCP tools map 1:1 to controller methods.
**Why:** No second Play protocol.
**Rejected:** CLI chaining across processes; leaving Player running.

### D5 — Save is `saveActiveScene`
**Choice:** MCP `save` and CLI Op `--save` call `EditorSceneEditSystem::saveActiveScene`. Not an Authorship Op. CLI Op without `--save` never calls `op`.
**Why:** Same persist as GUI Save. History stays the transform Command only.
**Rejected:** Save as Op; MCP Op `save` parameter; auto-save.

### D6 — MCP loop on Headless tick
**Choice:** After `initialize`, MCP reads stdin JSON-RPC (blocking or poll), dispatches, writes stdout. Engine still `tickOneFrame` so Vulkan/Play IPC can progress. CLI runs the verb after initialize (and enough ticks for GPU Capture / Play ready), then quits.
**Why:** Capture and Play frame need the render/Play pumps already in Headless `run`.
**Rejected:** One-shot Capture without starting systems; a second game loop.

## Risks / Trade-offs

- [MCP stdio vs engine tick] → Interleave read with tick; `waitForPlayFrame` already polls. Do not block forever on stdin during Play wait without also ticking — CLI episode uses wait helpers that poll IPC.
- [Base64 PNG size in MCP] → 1280×720 PNG is the product; accept context cost. No path fallback in v1.
- [Bare Headless vs adapter Headless] → Launch parse distinguishes `--mcp`/verb vs `--headless` only so Debug root and startup scene keep working for non-adapter Headless.
- [Minimal JSON-RPC] → Tests feed canned initialize/tools/call; do not claim full MCP compliance beyond those methods.

## Migration Plan

1. Extend `editor_launch` + tests (adapter flags, imply Headless, require root, `--scene`)
2. Shared dispatch + CLI JSON + PNG encode tests (fake stills / Authorship, no Vulkan)
3. Wire `engine_editor` main: CLI one-shot / MCP loop / skip startup scene
4. Play-frame episode on `PlaySessionController` hooks (existing session tests style)
5. Docs already in CONTEXT / ADR 0044; add testing notes if GPU smoke is needed
6. Rollback: revert launch flags and adapter sources; Headless boot unchanged

## Open Questions

None.
