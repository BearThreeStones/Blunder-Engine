## 1. Document History core

- [x] 1.1 Add Editor Command interface and Document History stack (cursor, execute/undo/redo, redo truncate, max depth 100)
- [x] 1.2 Implement dirty baseline cursor tied to successful save / undo-to-baseline
- [x] 1.3 Store and restore selection before/after snapshots on undo/redo
- [x] 1.4 Clear/replace Document History from `openScene`; wire into `RuntimeGlobalContext` / CMake
- [x] 1.5 Unit tests: stack limits, redo truncate, dirty baseline, selection restore

## 2. Soft-delete and export

- [x] 2.1 Add entity tombstone/soft-delete API on `SceneInstance` (stable EntityId; no mid-vector erase)
- [x] 2.2 Exclude tombstones from hierarchy visible tree and picking/render paths used by the editor
- [x] 2.3 Filter tombstones in `exportToScene` / save path
- [x] 2.4 Unit tests: soft-delete + undo identity; save omits tombstones

## 3. MVP Commands

- [x] 3.1 Implement SetEntityTransform Command; seal at gizmo drag end and Translate Modal confirm (cancel pushes nothing)
- [x] 3.2 Seal Inspector TRS commits into SetEntityTransform (Enter / focus-loss / spinner end)
- [x] 3.3 Implement SpawnEntity Command wrapping successful mesh spawn
- [x] 3.4 Implement SoftDeleteEntity Command for editor delete
- [x] 3.5 Unit tests for transform / spawn / soft-delete undo/redo round-trips

## 4. Input and UI

- [x] 4.1 Route Ctrl+Z and Ctrl+Y / Ctrl+Shift+Z to Document History (respect text-input claim)
- [x] 4.2 Add Edit menu Undo/Redo bound to the same API with canUndo/canRedo enable state
- [x] 4.3 Sync Inspector / hierarchy / viewport after undo/redo

## 5. Validation and docs

- [x] 5.1 Confirm ADRs `0006`–`0007` and `CONTEXT.md` match implementation
- [x] 5.2 Build with `vs2026-debug` (or documented preset) and run history-related tests
- [ ] 5.3 Manual smoke: transform confirm, spawn, delete, Ctrl+Z/Y, save omits deleted, openScene clears stack
