## Why

The v1 selection outline (`outline-selected`) delivers a working silhouette pass with fixed object-select orange, single selection, and hard 1-pixel edges. Blender's overlay stack goes further: transform-drag feedback via a distinct outline color, anti-aliased (smooth) outline edges, and multi-select with per-object ID encoding so multiple meshes outline correctly without spurious seams. Closing this gap improves editor parity and sets up future per-object styling without another render rewrite.

## What Changes

- **Transform-drag outline color**: While `TransformGizmoController` is actively dragging, the selection outline resolve color switches from object-select orange to Blender transform orange (distinct hue).
- **Smooth outline edges**: Extend `outline_resolve.slang` (and related uniforms) to anti-alias outline pixels—sub-pixel edge softening aligned with Blender `USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE` behavior for overlays, enabled by default.
- **Editor multi-selection**: Extend `EditorSelectionSystem` (and Hierarchy interaction) to support multiple selected entities; outline prepass draws all selected roots/subtrees.
- **Per-object ID encoding**: Prepass writes packed `(color_id, ob_id)` into the `R16_UINT` object-ID target; resolve detects silhouette edges against background and suppresses edges between co-selected objects that share the same `color_id`.
- **BREAKING (internal)**: `EditorSelectionSystem` gains a multi-ID API; call sites that assume a single `getSelection()` primary may need updates for modifier-key selection and inspector focus.

## Capabilities

### New Capabilities

- `outline-drag-color`: Transform-gizmo-drag outline color variant in the resolve pass.
- `outline-smooth-resolve`: Anti-aliased / smooth outline edge resolve (default on).
- `outline-multi-select`: Multi-entity editor selection and per-object ID prepass/resolve rules.

### Modified Capabilities

- `viewport-selection-outline`: Base outline requirements extended for multi-select, subtree batching per selection entry, and smooth resolve (supersedes single-ID constant `ob_id = 1` behavior from v1).

## Impact

- `engine/src/runtime/function/editor/editor_selection_system.{h,cpp}` — multi-select storage and API.
- `engine/src/runtime/function/slint/slint_system.cpp`, Hierarchy Slint UI — modifier-key selection (Shift add, Ctrl toggle typical pattern).
- `engine/src/runtime/function/render/overlay/outline_overlay.{h,cpp}` — per-selection draw list with packed IDs, drag color uniform.
- `engine/shaders/outline_prepass.slang`, `outline_resolve.slang` — packed ID output, color lookup, smooth edge kernel.
- `engine/src/runtime/function/render/overlay/overlay_state.h` — `is_transform_dragging` or equivalent flag from gizmo controller.
- `docs/agents/render-pipeline.md` — document ID packing and multi-select outline behavior.
