## Why

Add… is now the only product add path, but Inspector still draws empty Unique / Behaviours / Skeleton Modifiers foldouts (`No Skeleton on entity`, `No AnimationPlayer on entity`, empty headers). Authors see dead slots next to the picker. Second slice: show those sections only when something is actually attached.

## What Changes

- Hide Unique attachment property sections (Camera, Skeleton, Animation Player, Animation Tree) when that attachment is absent on the selection
- Hide Behaviours and Skeleton Modifiers sections when their lists are empty
- Remove empty-state copy such as `No Skeleton on entity` / `No AnimationPlayer on entity` — absence is “section not drawn”
- After Add… (including host cascade), newly created sections appear; after Remove (or undo), they disappear
- Add… catalog is unchanged: Unique rows stay listed and disable when present (do not hide catalog rows)
- Local Transform (and other non-Add… entity surfaces) stay as they are

**Out of scope:** changing Add… items, host cascade, hydration, Commands, Mesh in Add…, multi-select Add…, clip drag-from-browser, new ADR (presentation rule lives in CONTEXT + this spec)

## Capabilities

### New Capabilities
- `inspector-present-only-sections`: Inspector draws Add…-authored sections only when the selection actually has that attachment or list entry

### Modified Capabilities
- (none — `inspector-add-menu` is not yet in `openspec/specs/`; this slice adds a sibling capability rather than a delta on an unpublished spec)

## Impact

- `inspector_panel.slint`: wrap Unique / Behaviours / Skeleton Modifiers blocks in `if has-*` / `if length > 0`; drop empty placeholder Text
- Docked and floating Inspector share that panel; no new snapshot fields if flags already exist
- Docs: CONTEXT glossary term for present-only Inspector sections
- Validation: `engine_editor` + manual smoke (mesh-only entity vs Add Player cascade vs Remove)
