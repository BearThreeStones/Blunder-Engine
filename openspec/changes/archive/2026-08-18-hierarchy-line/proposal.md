## Why

Hierarchy Panel row names sit in the horizontal center of the panel, and expanded children do not read as a tree. Authors cannot scan parent/child at a glance. The panel already flattens a visible Scene Tree with depth and expand chevrons; it is missing left-aligned labels and **Hierarchy Line** gutter chrome.

## What Changes

- Left-align Hierarchy Panel entity names; same-depth names share one left edge
- Draw **Hierarchy Line** in the row gutter: per-depth vertical stem plus a horizontal tick into the expand-chevron column
- End a parent's stem at its last visible child; do not run that stem through grandchildren
- Keep an empty chevron slot on leaf rows so names stay aligned
- Keep the scene display name as panel chrome; root entity rows have no incoming line
- Treat the Hierarchy Line gutter as part of the row: pointer down on the row selects; pointer down on a chevron of a row with children toggles expand
- Use a muted gray line (`#737373`, 1px) that does not change on the selected row
- **Out of scope:** Content Browser tree guides, promoting the scene title to a tree root, drag-reparent, new hover chrome, selection-tinted or glowing lines

## Capabilities

### New Capabilities

- `hierarchy-panel`: Hierarchy Panel visible-tree listing, left-aligned names, Hierarchy Line grammar, and row hit testing

### Modified Capabilities

_(none)_

## Impact

- UI: `engine/src/runtime/function/slint/hierarchy.slint` (row layout, left-align, Hierarchy Line drawing); docked and floating Hierarchy Panel both consume `HierarchyTreeRow`
- Runtime: `EditorHierarchyTreeRow` / `HierarchySystem::rebuildVisibleTree` emit last-sibling / ancestor-continuation flags needed to draw stems; `SlintSystem::syncHierarchy` and floating-panel snapshot (`NativeFloatHierarchyRow` / `DockFloatingWindowHost`) copy those fields
- Docs: `CONTEXT.md` glossary already records Hierarchy Panel / Hierarchy Line (grilling); no ADR (visual chrome, easy to reverse)
- Tests: flatten logic for last-child stem stop and ancestor continuation (no first-party CTest suite yet — add a focused unit test if the engine test target can host it)
