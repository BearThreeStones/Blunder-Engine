## Why

Wiring idle/walk for Chocomel still means pasting AnimationClip GUIDs into AnimationPlayer rows and retyping the same stems on `PlayerMove` string fields. That fights the product model (Play by logical name; GUID is durable storage only; Import must not auto-fill the map) and lags Unity/Godot-style asset assign + named slots.

## What Changes

- Treat AnimationPlayer map rows as **Clip Bindings**: logical name → AnimationClip Asset Reference; authors assign clips via **Content Browser drop** or an **AnimationClip picker**; regular Inspector **hides GUID**
- **Add clip** opens the picker and, on confirm, appends one complete binding (logical name defaults to clip stem); cancel adds nothing; empty name+GUID drafts are not a product path
- Drop is **location-sensitive**: empty list / Add clip area appends; drop on an existing row retargets that row’s clip and **keeps** the logical name; per-row picker is always retarget-this-row
- Logical names are authorable aliases (may differ from stem); unique per player — append or rename that collides is **rejected** (no last-write-wins); two names MAY share one AnimationClip
- On scene load, discard rows where name and reference are both empty; keep half-filled rows and show them invalid
- Marked **Behaviour clip name** fields (explicit mark on the script member) use a dropdown of the co-located player’s logical names only — weak refs, no drop-on-Behaviour that mutates the map, no cascade when bindings rename/remove; empty map / no player → empty choice only
- **Out of scope:** Import auto-fill of the map (ADR 0031 / 0036); Behaviour-owned AnimationClip Asset References as the Play key; AnimationTree / Sync Group clip-name Inspector rewrites; advanced GUID disclosure

**BREAKING (authoring UX):** Add clip no longer creates an empty editable GUID row; pasting GUID is no longer the Inspector fill surface.

## Capabilities

### New Capabilities
- `clip-binding-authorship`: Clip Binding Inspector fill (picker, location-sensitive drop, alias uniqueness, hide GUID, load discard of dual-empty rows) and marked Behaviour clip-name dropdowns as weak logical-name refs

### Modified Capabilities
- `inspector-add-menu`: Replace “Add clip → empty name→GUID row” with “Add clip → picker → complete Clip Binding”; drop onto clip rows becomes in-scope for this capability’s companion rules (delta the Add clip requirement)
- `scene-edit-commands`: Add clip / clip retarget / clip-row Remove remain Document History Commands; Add clip’s after-state is a complete binding (or no-op on cancel), not an empty draft row

## Impact

- Slint AnimationPlayer section (`inspector_panel.slint`) + `slint_system` / dock floating host clip-row sync
- Clip-bindings Editor Commands (`makeSetAnimationPlayerClipBindingsCommand` and Add clip path)
- Content Browser drop classification: accept AnimationClip onto Player clip list / rows (today only mesh/scene)
- Behaviour Inspector: catalog/metadata mark for clip-name fields + dropdown over player map; property bag still stores string logical names
- Scene load/export: strip dual-empty bindings; uniqueness enforced on commit
- Tests: append vs retarget drop, collision reject, Add clip cancel, load discard, Behaviour dropdown / invalid weak ref
- Docs: CONTEXT (already grilled), [ADR 0036](../../../docs/adr/0036-clip-binding-authorship.md)
