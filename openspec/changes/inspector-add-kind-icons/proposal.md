## Why

Inspector Add… kinds are text-only. Behaviour rows, SkeletonModifier rows, Unique section headers, and Add… picker rows look alike, so authors scan names instead of kinds. Godot editor icons are already in-tree; Content Browser already wraps some of them.

## What Changes

- Show one **Add… kind icon** per kind: Camera, Skeleton, AnimationPlayer, AnimationTree, Behaviour, SkeletonModifier
- Placement: Unique section headers (between expand arrow and title); each Behaviour / SkeletonModifier row (left of type name, not on those section titles); matching Add… picker kind rows (not group labels)
- Color follows the adjacent label: normal ~`#d0d0d0`, Unique-already-present `#808080`, missing Behaviour `#ff9a9a`. Icon stays visible; do not swap assets
- No icon on clip rows, Transform, Mesh, Shading, or Hierarchy entities
- One icon per kind, not per CLR Behaviour type and not per SkeletonModifier subclass
- CONTEXT term **Add… kind icon** already recorded from grilling; no ADR

**Out of scope:** per-type Behaviour art; Modifier subclass icons; clip-row icons; Hierarchy entity type icons; new icon pack beyond `godot-icons`

## Capabilities

### New Capabilities
- `inspector-add-kind-icons`: Inspector and Add… picker mark each Add… kind with one editor icon at the agreed placements and colors

### Modified Capabilities
- (none — additive chrome; `inspector-add-menu` / `inspector-present-only-sections` requirements stay)

## Impact

- `editor_icons.slint`: wrap Camera3D / Skeleton3D / AnimationPlayer / AnimationTree / Script / SkeletonModifier3D
- `inspector_panel.slint`: Unique headers, Behaviour/Modifier rows, Add… kind rows
- Docked and floating Inspector share the panel
- Docs: CONTEXT already has the term
- Validation: `engine_editor` + visual smoke
