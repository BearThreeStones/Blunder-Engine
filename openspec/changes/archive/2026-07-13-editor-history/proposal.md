## Why

Transform, spawn, and delete edits in the editor write through immediately with no reversible history. Gizmo/Modal sessions already have clear confirm boundaries, but there is no Document History, so Ctrl+Z and “undo to last save” cannot exist. We need Editor History now so scene editing matches expected DCC behavior before more edit surfaces accumulate irreversible writes.

## What Changes

- Introduce **Editor History** / **Document History**: one linear command stack per active open scene
- Introduce **Editor Command** model (`execute`/`undo`/`redo`) with selection snapshots
- Wire **dirty** to a save baseline history cursor
- Soft-delete entities for undoable delete; **filter tombstones** on Save/`exportToScene`
- Ship MVP Commands: **SetEntityTransform**, **SpawnEntity**, **SoftDeleteEntity** (names may vary)
- Seal Commands only at interaction boundaries (gizmo/Modal confirm, Inspector commit)
- Cap stack at fixed depth (default **100**); truncate redo on divergent new edit
- Expose Undo/Redo via **Ctrl+Z**, **Ctrl+Y** / **Ctrl+Shift+Z**, and Edit menu
- Clear/replace Document History on `openScene`
- **Out of scope:** gameplay/runtime undo, branched history, generational EntityId, ObjectId command targets, persisting tombstones, cross-scene undo continuum

## Capabilities

### New Capabilities
- `document-history`: Document-scoped linear stack, dirty baseline, selection restore, input bindings, openScene reset
- `scene-edit-commands`: Transform / spawn / soft-delete Commands and save-time tombstone filtering

### Modified Capabilities
- *(none — no existing editor-history specs)*

## Impact

- New editor runtime area (e.g. `engine/src/runtime/function/editor/` history + commands) wired through `RuntimeGlobalContext` / UI host
- Touch points: `TransformGizmoController` confirm paths, Inspector TRS commit, `EditorSceneEditSystem` spawn/save/openScene, hierarchy visibility for tombstones, keyboard + Edit menu
- ADRs: `docs/adr/0006-editor-history-document-command-model.md`, `0007-editor-history-mvp-scope.md`
- Glossary: `CONTEXT.md` (Editor History terms); Transform undo (v1) note superseded when this ships
- Dense `EntityId` (`index+1`) unchanged; soft-delete avoids mid-vector erase
