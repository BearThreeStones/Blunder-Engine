## Context

See proposal.md for why. Grill stories are there. ADR 0053: Object Active is GameObject active, not Scene Visibility.

`Entity::m_enabled` is already the tombstone companion (`softDeleteEntity` sets it false). `Object::m_enabled` exists and is unused as product Active. Editor Camera `applyKeyboardFlyMovement` runs when free-look **or** `isCursorInViewport()` — hover-only WASD. Middle mouse is pan-only today (fly is skipped in that branch). v1 Play authorship patch JSON is a per-entity snapshot of Authorship Address + local TRS (`play_v1_entity_id`); `isPlayV1Patchable()` gates send.

World is Z-up. Do not change Gameplay Input WASD or Player Editor Camera policy.

## Goals / Non-Goals

**Goals:**

- Store Object Active on the scene entity without colliding with tombstone `Entity.enabled`.
- One `isActiveInHierarchy(EntityId)` used by gather/draw, pick, outline/gizmo, Light eval, Play Camera resolve, Behaviour Tick.
- One Document History Command for a selection (including multi-select align).
- Patch JSON carries Object Active; multi-entity Command sends one snapshot per id.
- Fly only while viewport-started RMB/MMB hold, including Q/E/Shift and MMB pan+fly together.

**Non-Goals:**

- C# `Object.Active` API this slice (Tick skip is enough).
- Scene Visibility eye.
- Changing scroll-zoom (still pointer-in-viewport).
- Rewriting descendants' flags.

## Decisions

1. **Entity `m_active` (default true), not `Entity.enabled`**  
   Persist as JSON `"active": false` when off; omit when on. Keep `Object::setEnabled` in sync at instantiate/bind so the existing Object flag matches. Tombstone still uses `m_enabled` / `m_tombstoned` only.  
   *Alternatives:* reuse `Entity.enabled` (breaks Delete undo); editor-only hide bit (rejected in Grill / ADR 0053).

2. **`SceneInstance::isActiveInHierarchy` walks parents**  
   False if tombstoned, locally inactive, or any ancestor is. Callers skip participation; they do not write child flags.  
   *Alternatives:* cache a dirty bit (later); store Active in Hierarchy (rejected).

3. **One `SetObjectActive` Command with a vector of (EntityId, before, after)**  
   Align computes the shared after value once. Label `Active` / `Inactive` with a name when single. `isPlayV1Patchable()` true. Extend `maybeSendPlayAuthorshipPatch` to send `buildPlayAuthorshipPatchJson` for each id on that Command (today: one `play_v1_entity_id`). Snapshot JSON adds `"active":true|false` beside `local`.  
   *Alternatives:* one Command per selected entity (N undo steps); a second Play socket.

4. **Hierarchy A is Slint `key-pressed` on the Hierarchy tree, not a global SDL hook**  
   Pointer-over-panel is then the widget that has the pointer. Viewport A stays available for fly during RMB/MMB. Inline Rename already consumes keys.  
   *Alternatives:* global SDL A + hit-test Hierarchy rect (duplicates dock/float geometry).

5. **Fly gate is `free_look || pan` after viewport-started button, not `isCursorInViewport()`**  
   Call `applyKeyboardFlyMovement` in both free-look and pan modes. Remove the hover-only OR. `isViewportInteracting` WASD without a button becomes false.  
   *Alternatives:* require pointer still in rect every frame (breaks capture); keep hover fly (rejected).

6. **Play Camera resolve / preflight use Active in Hierarchy**  
   Same helper as draw. Inactive Main is skipped; all-inactive fails Play with the existing no-Camera report.  
   *Alternatives:* keep using inactive Main (would render a camera whose Object is off).

## Risks / Trade-offs

- [Existing scenes omit `"active"`] → Treat missing as on; no migration.
- [Patch send is still TRS-shaped] → Additive `"active"` key; Player apply ignores missing key (leave current Active).
- [Multi-select patch] → One sendPatch per entity; order stable EntityId. Failed one still Warning, no History rollback.
- [A vs fly] → Hierarchy widget consumes A only when pointer is there; fly A only during button hold — no overlap.
- [Checkbox vs Interior freeze 22px rows] → Checkbox in the name row, left of the name, after chevron; keep 22px.

## Migration Plan

1. Land persist + `isActiveInHierarchy` + participation skips + Command/UI + fly gate + patch field.
2. No scene JSON rewrite. Authors who never toggled stay all-on.
3. Rollback: revert; scenes with `"active": false` would load as on if the parser is reverted — acceptable.

## Open Questions

None.
