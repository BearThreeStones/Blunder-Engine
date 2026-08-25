## Context

See proposal.md for motivation. Grilling locked CONTEXT **Hierarchy row icons** and **Attachment property preview**. Hierarchy rows today are `HierarchyTreeRow` (id, name, depth, expand flags) rebuilt in `HierarchySystem` with no attachment payload. Row hit testing is a full-row `TouchArea` in `hierarchy.slint` (chevron vs select). Inspector already has Unique/Behaviour/Modifier/Transform sections and Godot wrappers in `editor_icons.slint` (`EditorIconCamera3D`, `EditorIconLight`, `EditorIconSkeleton3D`, `EditorIconAnimationTree`, `EditorIconScript`, `EditorIconSkeletonModifier3D`, `EditorIconMesh`). Transform uses `EditorGlyphs.tool-move`. Camera Preview is a separate viewport PiP driven by selection, not by Hierarchy Alt+LMB.

## Goals / Non-Goals

**Goals:**
- Per-visible-row icon list from scene/Object state at Hierarchy rebuild
- Right-aligned icon strip; LMB vs Alt+LMB without breaking chevron/gutter/Create…
- Floating preview cards that edit through the same Document History Commands as Inspector
- Docked and floating Hierarchy share the strip

**Non-Goals:**
- Removing AnimationPlayer from ClassDB / Add… / scene JSON (no icon if still present)
- Clip Binding Hierarchy icons
- Extracting Inspector into a shared Slint component library beyond what this slice needs
- ADR

## Decisions

### D1 — Icon slots on the tree row, not a second query in Slint
**Choice:** Extend `EditorHierarchyTreeRow` with an ordered list of icon slots: kind + list index (Behaviour / SkeletonModifier ordinal). C++ fills slots while appending visible rows (same sources Inspector uses: Camera/Light components, MeshRenderer, Skeleton/Tree on bound Object, Behaviour list, modifier chain). Slint only draws.
**Why:** Floating Hierarchy already clones row structs in `dock_floating_window_host.cpp`; one payload stays consistent.
**Rejected:** Slint-only bools (`has-camera`…) — Behaviour/Modifier duplicates need indices. Per-frame Inspector snapshot fan-out into every Hierarchy row.

### D2 — Product Uniques only
**Choice:** Emit Camera, Light, Skeleton, AnimationTree. Never emit AnimationPlayer or Clip Binding, even if `hasAnimationPlayer()` or the Tree clip map is non-empty.
**Why:** Glossary: Player Unique retired; Clip Binding is Tree-map rows, not scene mounts.
**Rejected:** Showing Player “because the engine still has it”; one clip-library icon.

### D3 — Icon `TouchArea`s, rest of row unchanged
**Choice:** Name + gutter + chevron keep current handlers. Each icon is its own `TouchArea` on the right. Plain left-down: `entity-selected` only. Alt+left-down: select + preview callback `(entity-id, kind, index)`. Right-down on an icon: same as right-down on the row (single-select + Create… menu), not preview.
**Why:** Matches grilled LMB vs Alt+LMB; avoids chevron false hits (icons are far right).
**Rejected:** Full-row Alt+LMB; LMB on icon opens preview.

### D4 — Alt detection
**Choice:** Use Slint `PointerEvent` modifiers if this Slint fork exposes Alt; otherwise read editor Alt key state in the C++ callback (same frame as the click). Do not require a hold-to-peek timer.
**Why:** Product is Alt+LMB, not hover delay.
**Rejected:** Ctrl+LMB; double-click.

### D5 — Cards are editor floating chrome, not Camera Preview
**Choice:** Host preview cards on `editor_window` (and keep them if Hierarchy is floating). Each card is entity-id + kind + index, fields bound like that Inspector section (reuse existing edit callbacks / Commands). Initial position near the clicked icon. Pin only changes lifetime vs selection, not dock auto-hide.
**Why:** Pinned cards must survive Hierarchy scroll and selection change; Camera Preview stays selection-driven PiP.
**Rejected:** In-list popup clipped by the tree Flickable; retargeting Camera Preview from the Camera icon.

### D6 — One card key; Inspector Commands
**Choice:** Card identity is `(entity-id, kind, index)`. Unpinned toggle-close and pinned raise use that key. Field commits call the same Inspector Command path (Document History). If the preview card has keyboard focus, Focus-routed Undo treats it as Document History (not Asset Inspector Global).
**Why:** Grilled undo + no duplicate pinned cards.
**Rejected:** Read-only cards; a third history stack.

### D7 — Drop cards on gone / document swap
**Choice:** Rebuild or subscribe: if entity tombstoned, attachment missing, or `openScene` replaces Document History, close matching cards. Entering Play Mode does not close cards.
**Why:** Matches glossary lifetime. Play uses a separate simulation instance; authorship rows remain.
**Rejected:** Stale pinned cards; restoring pins on Undo or scene reopen.

## Risks / Trade-offs

- [Wide icon strip on deep names] → Let the name elide; icons stay right-aligned and clickable; Transform always present will crowd empty rows (accepted)
- [Slint Alt missing] → C++ modifier fallback; test both docked and floating Hierarchy
- [Duplicating Inspector field UI] → Prefer wrapping existing foldout bodies; if too coupled, bind the same properties with a thinner card chrome
- [Engine still has AnimationPlayer] → Hide icon only; do not migrate scenes in this change

## Migration Plan

1. Row payload + rebuild
2. Hierarchy Slint strip + hit testing
3. Preview card host + pin/lifetime
4. Wire edits + Undo focus
5. Build `engine_editor`; smoke docked + floating Hierarchy

Rollback: revert editor UI/C++ row fields; no Asset format change.

## Open Questions

None.
