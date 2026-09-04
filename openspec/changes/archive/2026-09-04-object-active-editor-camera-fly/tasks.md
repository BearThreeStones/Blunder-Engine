## 1. Persist Object Active

- [x] 1.1 Add `Entity` Object Active (`m_active`, default true) distinct from tombstone `m_enabled`. Sync `Object::setEnabled` on instantiate/bind.
- [x] 1.2 Scene JSON: omit `"active"` when on; write `"active": false` when off; missing means on. Round-trip in serializer.
- [x] 1.3 `SceneInstance::isActiveInHierarchy(EntityId)` — false if tombstoned, locally inactive, or any ancestor is.

## 2. Participation

- [x] 2.1 Skip gather/draw, outline, and Transform gizmo for entities that are not Active in Hierarchy.
- [x] 2.2 Viewport pick / peel / piercing omit those entities.
- [x] 2.3 Light eval treats them as not contributing (same as Light enabled off; no cap slot).
- [x] 2.4 Behaviour Tick skips Objects that are not Active in Hierarchy.
- [x] 2.5 Play Camera resolve and Play preflight skip Cameras that are not Active in Hierarchy (no Editor Camera fallback).

## 3. Command and Play patch

- [x] 3.1 `SetObjectActive` Command: vector of (EntityId, before, after); one History step; multi-select align (all off iff every selected is on).
- [x] 3.2 Patch JSON includes `"active"`; apply writes Object Active. `maybeSendPlayAuthorshipPatch` sends one snapshot per entity on that Command.

## 4. Hierarchy and Inspector

- [x] 4.1 Hierarchy row checkbox (local Object Active) + grey name when not Active in Hierarchy. Checkbox does not expand or rename. Unselected row: select single then toggle.
- [x] 4.2 Hierarchy `key-pressed` A while pointer over the panel runs the toggle; Inline Rename types A; viewport/Inspector A does not toggle.
- [x] 4.3 Inspector identity checkbox beside the name; mixed selection indeterminate; same Command.

## 5. Editor Camera fly

- [x] 5.1 Fly WASD/Q/E/Shift only in `free_look` or `pan` after viewport-started RMB/MMB. Remove hover-only `isCursorInViewport` fly. Pan still pans. Capture may leave the rect.

## 6. Tests

- [x] 6.1 `object_active_test`: persist, parent toggle keeps child flags, `isActiveInHierarchy`, Command undo, multi-select align.
- [x] 6.2 Extend `play_camera_resolve_test` (inactive Main skipped; all inactive not ok) and `play_authorship_patch_test` (`active` round-trip).
- [x] 6.3 Extend `light_eval_test` (inactive light ignored). Editor Camera fly: hover does not move; RMB hold + W does.

## 7. Docs

- [x] 7.1 ADR 0053 is in `docs/adr/`. Keep `CONTEXT.md` terms in sync if implementation names differ.
