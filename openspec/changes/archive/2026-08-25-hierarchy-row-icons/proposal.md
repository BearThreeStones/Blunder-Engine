## Why

Hierarchy entity rows are name-only. Authors cannot see Transform, MeshRenderer, Unique attachments, Behaviours, or SkeletonModifiers without selecting and scanning Inspector. Unity-style far-right kind icons plus Alt+LMB property peek are the locked product for that scan, distinct from Camera Preview.

## What Changes

- Draw **Hierarchy row icons** at the right of each entity row: Local Transform (always); MeshRenderer when present; Unique Camera / Light / Skeleton / AnimationTree when present; each Behaviour; each SkeletonModifier (list order). Reuse **Add… kind icon** art for Unique / Behaviour / SkeletonModifier. Transform and MeshRenderer are row-only (not Add… kinds)
- No Clip Binding icons (bindings live on AnimationTree Inspector). No AnimationPlayer Unique icon (product Unique retired)
- Plain LMB on an icon selects that row’s entity only — same as clicking the name. Does not open a preview
- Alt+LMB on an icon opens **Attachment property preview**: a floating Inspector-like card for that one attachment. Camera Unique icon still opens this card, not Camera Preview
- Card pin locks that entity+attachment across selection change (not dock auto-hide pin). Unpinned: selection change or Alt+LMB another icon closes it. Alt+LMB same icon: unpinned closes; pinned raises (no duplicate). Multiple pinned cards allowed
- Card field edits are Document History (including pinned card whose entity is not the current selection). Preview-card focus uses Document History Undo, not Global
- Card closes when its entity/attachment is gone, or when the scene document closes/switches. Undo restore does not reopen. Entering Play Mode does not close cards

**Out of scope:** deleting AnimationPlayer ClassDB / scene format; moving Clip Binding / TimeScale (already glossary); Inspector Add… icon work (`inspector-add-kind-icons`); Camera Preview PiP behavior except the Alt+LMB collision; Mesh Preview; Edit animation preview / Fire slot

## Capabilities

### New Capabilities
- `hierarchy-row-icons`: Far-right Hierarchy entity-row icons for Transform, MeshRenderer, Uniques, Behaviours, and SkeletonModifiers
- `attachment-property-preview`: Alt+LMB floating Inspector-like card, pin, Document History, lifetime vs selection / document / Play

### Modified Capabilities
- `hierarchy-panel`: Row hit testing — LMB on the icon strip selects; Alt+LMB opens attachment preview; chevron/gutter rules unchanged
- `document-history`: Preview-card edits and preview-card focus route to Document History
- `camera-preview`: Alt+LMB on the Hierarchy Camera icon SHALL NOT open or retarget Camera Preview

## Impact

- `hierarchy.slint` / `HierarchyTreeRow` / `HierarchySystem` / `slint_system.cpp` / floating Hierarchy host: per-row icon payload + click vs Alt+click
- New preview chrome (Slint popup/window) bound to one Inspector section’s fields
- Document History + Focus-routed Undo when the card has focus
- Docked and floating Hierarchy share the row chrome
- Docs: CONTEXT terms already recorded from grilling; no ADR for this chrome (animation Unique retirement is a separate change)
- Validation: `engine_editor` + visual / interaction smoke
