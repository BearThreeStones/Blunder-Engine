## Why

Authorship v1 and Host observation v1 exist as C++ APIs, and Headless Editor already boots without a window. Agents and CI still have no process entry: Capture, Play step, Play frame, Query / Op / Diagnose, and Play Session stay in-process only.

## What Changes

- Ship **CLI** and **MCP** as two presentations of one verb set on `engine_editor` (not a third process, not Player-hosted, not MCP-as-domain)
- Adapters wrap Authorship v1, Host observation v1, and existing Play Session verbs. Stills stay observation; they do not become Query
- MCP: stdio JSON-RPC on a long-lived Headless Editor Session. No listen port
- CLI: one verb, one process, then exit. Play observation is a `play-frame` episode (compose Play / optional step / one Play frame / Stop)
- `--mcp` or a CLI verb implies Headless. Windowed + adapter fails closed. Bare `--headless` without an adapter stays valid
- `--project-root` required for adapters (no `BLUNDER_PROJECT_ROOT` fallback). `--scene` required for a Live document
- CLI Op requires `--save`. MCP has a `save` verb (editor persist, not an Op). MCP Op has no save parameter
- Stills: CLI `--out` PNG; MCP ImageContent (PNG). CLI stdout is one JSON object; stderr is logs. Request failure is non-zero exit; Diagnose that ran is exit 0 with Issues in JSON

**Out of scope:** Play dump; HTTP / attach to a windowed editor; `engine_agent`; `EngineHostMode::Headless`; CLI JSON REPL; new Authorship Ops (Import / Cook / Play-start); changing Capture aspect or Play step dt

## Capabilities

### New Capabilities
- `machine-adapters`: CLI and MCP presentations on Headless `engine_editor` — launch rules, shared verb set, stdio MCP, one-shot CLI, still encoding, JSON / exit mapping, document Save as adapter persist

### Modified Capabilities
- `authorship-contract`: CLI and MCP adapt this contract from the Editor Session process; Live requires `--scene`; CLI Op persist via `--save`; MCP `save` is not an Op
- `host-observation`: adapters emit Capture / Play frame as PNG (CLI `--out`, MCP ImageContent); CLI `play-frame` is the episode presentation of the same Play frame product
- `headless-host`: `--mcp` / CLI verb imply Headless; adapter launches require `--project-root`
- `play-mode`: Headless Play Session remains the backend for MCP granular play verbs and the CLI `play-frame` episode

## Impact

- `editor_launch` parse (`--mcp`, `--scene`, CLI verb, imply Headless, adapter `--project-root`)
- `engine_editor` main: adapter mode vs tick loop vs one-shot then quit
- New adapter dispatch (shared verbs) calling Authorship System, `captureScene`, `PlaySessionController`, scene Save
- PNG encode of Capture / Play frame RGBA
- Tests: launch parse, dispatch fail-closed codes, CLI JSON (no Vulkan); optional Headless smoke later
- Docs: CONTEXT glossary, [ADR 0044](../../../docs/adr/0044-machine-adapters.md)
