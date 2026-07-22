## Why

Document History already supports undo/redo, but authors cannot see or scrub the stack in the UI. DCC users expect a History Panel next to the filesystem browser, with room for non-document (Global) actions later. Shipping the panel now—with an empty Global History placeholder—makes scene undo discoverable without inventing Global Commands yet.

## What Changes

- Add a **History Panel** as a sibling tab to Content Browser in the same dock tab group
- Introduce **Global History** as a separate empty stack (placeholder; no Global Commands this milestone)
- Add **History scope filter** (Scene / Global checkboxes; default both on; both on still shows only Scene—no merge)
- Expose Document History entries with English **Command labels** (action + snapshotted entity name)
- Support **History Jump** on click (undo/redo to the chosen cursor)
- Visual **History row state**: oldest-top; redo tail muted; current position highlighted
- Keep Ctrl+Z / Redo / Edit menu bound to **Document History only**
- **Out of scope:** Global Commands, interleaved Scene/Global timeline, focus-based shortcut routing, localization of labels

## Capabilities

### New Capabilities
- `history-panel`: History Panel UI, scope filter, row presentation, History Jump
- `global-history`: Empty Global History stack wired into the editor (placeholder API)

### Modified Capabilities
- `document-history`: Commands gain display labels (snapshotted names); panel reads stack for listing/jump (no change to linear stack semantics)

## Impact

- UI: new Slint History Panel + Content Browser sibling tab; dock/`DockPanelKind` if needed
- Runtime: `GlobalHistory` (or equivalent) in `RuntimeGlobalContext`; `IEditorCommand` label support; Document History query API for listing/seek
- Touch points: `ui_host` / presentation sync, existing MVP commands (`SetEntityTransform`, `Spawn`, `SoftDelete`)
- Docs: ADR `0008-history-panel-and-global-history.md`; `CONTEXT.md` glossary (already updated)
- Tests: label snapshot, jump seek, filter defaults, Global empty behavior
