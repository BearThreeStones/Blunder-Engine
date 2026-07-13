## Context

Blunder’s editor mutates the active `SceneInstance` in place (gizmo, Translate Modal, Inspector TRS, mesh spawn) and tracks a boolean dirty flag on `EditorSceneEditSystem`. There is no command stack. `EntityId` is dense (`index + 1`); mid-vector erase would invalidate IDs. ADRs `0006`–`0007` and `CONTEXT.md` lock Document History, Command boundaries, soft-delete, dirty baseline, and MVP scope. Reflection kernel (`ObjectId`) exists but scene editing still uses `EntityId`.

## Goals / Non-Goals

**Goals:**
- Ship a testable `EditorHistory` / Document History owned by the active scene
- Make transform commits, spawn, and soft-delete undoable/redoable with selection restore
- Align dirty with save baseline; filter tombstones on export/save
- Wire Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z and Edit menu to the same API

**Non-Goals:**
- Gameplay/runtime rewind; branched undo trees
- Generational EntityId or physical delete reclaim
- ObjectId-targeted Commands; reparent-as-first-class Command
- Persisting tombstones or undo stacks across `openScene`

## Decisions

1. **Owner** — `EditorHistory` (or equivalent) lives under editor function code, held by `RuntimeGlobalContext` / editor services beside `EditorSceneEditSystem`. Document History is cleared when `openScene` replaces the active document.
2. **Command interface** — Abstract Editor Command with `undo()`/`redo()` (and execute-on-push). Concrete commands store enough snapshot data (TRS, spawn payload, soft-delete flag, selection before/after) to reverse without a full scene Memento.
3. **Seal points** — Gizmo drag end and `confirmTranslateModalSession` push one transform Command (before = drag/session start pose). Inspector pushes on field commit only. Spawn wraps `spawnMeshAsset` success. Delete marks tombstone + pushes SoftDelete Command.
4. **Soft-delete storage** — Prefer entity-local tombstone/disabled + hierarchy/export filters over removing from `m_entities`. `exportToScene` / save skips tombstoned entities.
5. **Dirty baseline** — On successful save, record current history cursor (or stack generation index). `isDirty` is true iff cursor ≠ baseline (or stack mutated past baseline). `markDirty` from legacy paths remains until all edits go through Commands; Commands update dirty via history.
6. **Stack** — Linear deque/vector + cursor; max 100; drop oldest on overflow; new execute after undo clears redo tail.
7. **Input** — Editor key routing (not gameplay) handles shortcuts when focus is not exclusively in a text field that owns Undo; menu items call the same `undo`/`redo`. Enable state from `canUndo`/`canRedo`.

## Risks / Trade-offs

- **[Risk] Mixed markDirty bypasses history → Mitigation:** Migrate gizmo/Inspector/spawn/delete call sites in this change; audit remaining `markDirty` for non-document noise.
- **[Risk] Soft-delete leaks into pick/render → Mitigation:** Tombstones excluded from hierarchy, picking, and mesh draw the same way as disabled/hidden editor entities.
- **[Risk] Dense ID + soft-delete growth → Mitigation:** Accept session growth for v1; physical reclaim later with generational IDs (ADR 0007).
- **[Risk] Shortcut fights Slint text Undo → Mitigation:** Only handle when editor viewport/chrome has focus or when no text input claims the shortcut.

## Migration Plan

1. Land history core + unit tests (stack, dirty baseline, truncate redo)
2. Add Commands + soft-delete/export filter; wire spawn/delete/transform seal points
3. Wire shortcuts + Edit menu; clear history on `openScene`
4. Rollback: feature unused if unwired; no scene format change (tombstones never serialized)

## Open Questions

- Exact Inspector commit hook location in Slint bridge (Enter / focus-loss path to wire)
- Whether Cancel Translate Modal should avoid pushing history (expected: yes — cancel restores start pose without Command)
