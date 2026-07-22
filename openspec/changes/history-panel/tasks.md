## 1. History stack APIs

- [x] 1.1 Add Command label support on `IEditorCommand` / factories (English action + snapshotted entity name; type-only fallback)
- [x] 1.2 Expose Document History list + `seekTo` (undo/redo loop) for the panel
- [x] 1.3 Add `GlobalHistory` (same stack shape), wire into `RuntimeGlobalContext`; leave empty (no Global Commands)
- [x] 1.4 Unit tests: label snapshot vs rename; seek undo/redo; Global remains empty when Document pushes

## 2. History Panel UI

- [x] 2.1 Add History Panel Slint (Scene/Global filters default on; oldest-top list; muted redo tail; current highlight)
- [x] 2.2 Place History as sibling tab to Content Browser in the same tab group
- [x] 2.3 Sync panel rows from Document/Global history; wire click → History Jump; respect filter rules (both on → Scene only)
- [x] 2.4 Mark panel dirty on history push/undo/redo/openScene; keep shortcuts on Document History only

## 3. Validation and docs

- [x] 3.1 Confirm ADR `0008` and `CONTEXT.md` match implementation
- [x] 3.2 Build with `vs2026-debug` (or documented preset) and run history-related tests
- [ ] 3.3 Manual smoke: History tab beside filesystem, labels, jump, filters, Ctrl+Z still scene-only
