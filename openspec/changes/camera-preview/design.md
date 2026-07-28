## Context

Camera Component, Play resolve (`resolvePlayCamera` / FOV+near/far+world → view/proj), Camera Gizmo, and single-offscreen → Slint `viewport-image` already ship. There is no second editor view. Viewport chrome already splits Slint overlays (`TransformToolbar`) vs Vulkan screen overlays (Navigate gizmo). Grilling (2026-07-28) locked visibility, live render, Slint chrome, no overlays in preview, Unity-like drag/resize/collapse, aspect from content box, ≤480 long edge, in-process layout memory.

## Goals / Non-Goals

**Goals:**

- Live Camera Preview PiP when selection includes a Camera Component.
- Slint chrome + independent preview image; block viewport input over the panel.
- Overlay-free secondary render from Camera pose/FOV/clips; aspect from content box.
- Editor-only; Player never shows it.

**Non-Goals:**

- CameraComponent.aspect / sensor size field.
- Maximize / lock / other Unity menu items beyond Collapse.
- Persisted editor prefs for panel layout.
- Docked Game View / multi-camera simultaneous previews.
- Editor Overlays inside the preview.
- Zero-copy present for the preview image in this slice (CPU readback OK).

## Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Visibility | Any selected Camera; target = primary-if-Camera else first selected Camera | Grilled (C) |
| Content | Live secondary scene render | Grilled (C) |
| Chrome layer | Slint panel + second `image` | Grilled (B); title/drag/menu need UI |
| Overlays in preview | None | Grilled (A); Game-like |
| Interaction | Drag, resize, collapse; menu = Collapse only; block pick under panel | Grilled |
| Aspect | Preview content box size | Grilled; matches Play RT aspect rule; no new component field |
| Perf | Same frame; longest edge ≤480; stop when collapsed | Grilled |
| Persistence | In-process only | Grilled |
| Shadow in preview | `shadows_enabled = false` for preview frame | Cost; shadow map is shared/global |
| Present | CPU readback → `setCameraPreviewImage` | Avoid complicating zero-copy main path |
| Forward path | `ForwardRenderPath::renderFrameTo(target, …, draw_overlays=false)` | Reuse mesh draws; skip OverlaySystem |

## Risks / Trade-offs

- [Double scene cost] → Cap RT at 480 long edge; skip shadows/overlays; stop when collapsed.
- [ForwardRenderPath single `m_offscreen`] → Explicit `renderFrameTo` avoids dual path ownership of descriptor pools.
- [Slint hit vs Vulkan pick] → Sync panel rect to C++ every frame (same pattern as top-right chrome).
- [Idle skip / dirty frame] → Preview must force viewport render when panel visible and camera/scene dirty (reuse `m_force_viewport_render` or equivalent).

## Migration Plan

None. Selecting a Camera starts showing the panel; no asset format change.

## Open Questions

None — grilled and confirmed 2026-07-28.
