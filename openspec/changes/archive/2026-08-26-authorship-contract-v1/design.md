## Context

See proposal.md for motivation. Grilling locked CONTEXT terms and [ADR 0041](../../../docs/adr/0041-authorship-contract.md). Scene edits already go through `IEditorCommand` / `DocumentHistory`; transform is `makeSetEntityTransformCommand` (apply, then `push`). Commands still target `EntityId`. Scene files and `SceneInstance::findEntityByName` already use unique names; ObjectId is a session handle on save. Play camera gate is `runPlayCameraGate` on a loaded `Scene` in `startPlaySession`; Scripts gate is `runPlayScriptsGate` inside `PlaySessionController::play`. Both return `std::string error` today. Editor systems currently start even on Player host; this slice still must **not** create Authorship System in Player.

## Goals / Non-Goals

**Goals:**
- C++ Authorship contract API that tests call without Slint, MCP, or a CLI
- Live Query/Op via Authorship System + Document History
- Shared Play-rule Diagnose used by the API and by Play start
- Play last-error UI can keep a human string copied from the first Error Issue

**Non-Goals:**
- JSON-RPC, CLI, or MCP adapters
- Headless Editor Session / no-window host
- New Command types or a Command catalog Seam
- Changing EntityId inside Commands
- Partial-TRS Ops (v1 always writes full local TRS)

## Decisions

### D1 — C++ structs, not a wire protocol
**Choice:** `AuthorshipQuery` / `AuthorshipOp` / `AuthorshipDiagnose` (names may vary) return `Request failure` code or payload / Issue vector. Tests link `engine_runtime` like `editor_commands_test`.
**Why:** Adapters come later; the kernel must exist first.
**Rejected:** JSON-RPC in-process; MCP tools in this slice; scraping logs.

### D2 — String Issue codes
**Choice:** Stable codes `play.missing_camera`, `scripts.dirty`, `scripts.missing_output`, `scripts.build_failed`. Request failures `address.unknown`, `subject.live_required`, `subject.no_live_document`, `subject.scene_unreadable`.
**Why:** Agents assert codes; explanations can change.
**Rejected:** Enum-only codes with no stable string; Console Message text as the contract.

### D3 — Shared Play-rule functions, System is the live facade
**Choice:** Pure functions Diagnose a `Scene` (disk or exported live) plus Project scripts dirtiness (`areProjectScriptsDirty` and missing-DLL check) into Issues. Authorship System binds Live document (`EditorSceneEditSystem` active `SceneInstance` + `DocumentHistory`). On-disk path deserializes the Scene Asset (virtual path) without the System. Play `startPlaySession` calls the same Camera Diagnose after the existing dirty-prompt / `loadScene` path; `PlaySessionController` maps build failure to `scripts.build_failed`.
**Why:** One rule implementation; Player never needs the System.
**Rejected:** Duplicating camera checks in UI vs API; making Diagnose compile Scripts.

### D4 — Editor-only mount
**Choice:** Create Authorship System in `RuntimeGlobalContext::startSystems` when host is Editor, not Player. Put sources under `engine/src/runtime/function/editor/` next to Scene Edit. Callers tolerate null (Registered System).
**Why:** ADR 0041; Op requires Document History.
**Rejected:** Context System; mounting in Player; Privileged core.

### D5 — Name resolution matches editable document
**Choice:** Resolve via `findEntityByName`, then reject tombstoned and empty names as `address.unknown`. Name list walks entities and skips `isOmittedFromDocument` / empty names. Live Camera Diagnose inspects `SceneInstance` cameras for subject=Live, and the Play-entry `Scene` for Play start / subject=On-disk.
**Why:** Matches Hierarchy / save export; Play start already gates the loaded asset, not unsaved Add Camera unless Save and Play ran.
**Rejected:** Hierarchy paths; EntityId on the API; treating tombstones as addressable.

### D6 — Transform Op reuses apply-then-push
**Choice:** Read current local TRS, write new TRS, `push(makeSetEntityTransformCommand(...))` with current selection snapshots. Full TRS each Op.
**Why:** Same seal as Inspector/gizmo; one Command.
**Rejected:** New Command type; Euler as source of truth; mutating without History.

### D7 — Play errors stay displayable
**Choice:** Keep `PlaySessionController` / `setLastError` string as the first Error Issue explanation (or existing hook/spawn strings that are not Diagnose). Also store the Issue list when the failure came from Diagnose or Scripts build.
**Why:** Toasts/log already read `lastError`; do not require Slint work to land the kernel.
**Rejected:** Removing the string field in this slice; a second Play-only error type.

## Risks / Trade-offs

- [Player host still constructs other editor systems] → Authorship System is the one this slice gates; do not expand the Host split.
- [Live Diagnose vs Play start disagree on unsaved Camera] → Document the subject: Live Query/Diagnose see the Live document; Play start Diagnoses the scene that will load after save rules (today's `loadScene`).
- [Name collisions in `m_name_to_id`] → Follow existing last-wins table; do not invent uniqueness repair here.
- [Scripts dirty vs missing output overlap] → If no game DLL, emit `scripts.missing_output` and not `scripts.dirty`; if DLL exists but sources are newer, emit `scripts.dirty` only.
- [Tests linking SceneInstance need slang/slint on PATH] → Same as `editor_commands_test`; keep tests off Vulkan.

## Migration Plan

1. Issue / Request failure types + Play-rule Diagnose functions; extend `play_preflight_test`
2. Authorship System + Live Query/Op; `authorship_contract_test`
3. Wire Play start Camera + Scripts build failure to Issues
4. Skip System on Player host
5. Rollback: revert new system and Play wiring; no scene format change

## Open Questions

None.
