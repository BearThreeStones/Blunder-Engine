## Context

Document History (`DocumentHistory` + MVP Commands) already powers Ctrl+Z / Edit menu. There is no History Panel UI and no Global History stack. ADR 0008 and the grilled glossary define dual stacks, panel placement, labels, jump, and filter rules. Content Browser is a dock panel (`DockPanelKind::content_browser`); History must be a sibling tab in that group.

## Goals / Non-Goals

**Goals:**
- Ship History Panel as sibling tab to Content Browser
- List Document History with English Command labels and History Jump
- Introduce empty Global History + Scene/Global filters (default both on; both on → Scene only)
- Keep shortcuts on Document History only

**Non-Goals:**
- Global Commands or focus-based shortcut routing
- Interleaved Scene/Global timeline merge
- Localization of Command labels
- Branched history or changing Document History stack semantics

## Decisions

1. **Dual stacks** — Keep `DocumentHistory` for scene Commands. Add `GlobalHistory` with the same linear stack shape (reuse or thin wrapper over the same command-stack type). Wire into `RuntimeGlobalContext`. Global stays empty this milestone.
2. **Command labels** — Extend `IEditorCommand` with a virtual `label()` (or stored `eastl::string` set at make-time). Factories snapshot entity display name at push. English phrases: `Move {name}`, `Spawn {name}`, `Delete {name}` with type-only fallbacks.
3. **Listing / seek API** — Document History exposes read access: command count, cursor, label at index, and `seekTo(index)` that loops undo/redo. Panel never reimplements undo.
4. **Panel chrome** — New Slint component (e.g. `history_panel.slint`) embedded beside Content Browser via an in-panel tab strip (File System | History) **or** a second dock widget in the same tab group—prefer matching existing dock tab patterns. Default layout opens both tabs in one group; Content Browser remains default active.
5. **Filter UI** — Scene/Global checkboxes; session state. When Global is checked alone and empty → empty list. When both checked → show Document History only (no merge).
6. **Row state** — Indices `< cursor` (and the last applied) full opacity; `>= cursor` redo tail muted; highlight current cursor position. List order oldest-top.
7. **Dirty / openScene** — Unchanged for Document History. Global History is session-scoped; opening a scene does not need to clear Global (empty anyway); document clear rules stay document-only.
8. **Dock kind** — Prefer embedding History inside the content-browser panel chrome as a local tab to avoid a new `DockPanelKind` and default-layout churn; if that fights existing Slint structure, add `DockPanelKind::history` co-located in the same tab group.

## Risks / Trade-offs

- **[Risk] Empty Global looks unfinished → Mitigation:** Filters and empty state are intentional placeholder per ADR 0008; no fake Global rows.
- **[Risk] Seek loops N undo/redo on long stacks → Mitigation:** Accept for v1 (max depth 100); optimize later if needed.
- **[Risk] Sibling-tab vs new DockPanelKind mismatch → Mitigation:** Inspect current Content Browser Slint/dock wiring first; pick the smaller integration path (Decision 8).
- **[Risk] Label missing on older commands → Mitigation:** All MVP factories set labels in this change; tests cover snapshot rename.

## Migration Plan

1. Stack read/seek + labels + GlobalHistory + unit tests
2. History Panel Slint + sync/jump wiring
3. Default layout / tab sibling to Content Browser
4. Rollback: hide tab / leave Global unused; no asset format change

## Open Questions

- Exact Slint embedding: in-panel tabs vs `DockPanelKind::history` (resolve during task 1 UI spike)
