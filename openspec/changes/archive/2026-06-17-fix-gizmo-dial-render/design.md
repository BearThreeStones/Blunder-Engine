## Context

`fix-transform-gizmo` shipped mode persistence (option A), thicker translate arrows in shader, scale box geometry, and scale drag logic. User testing shows:

| Style | Enum | Observed |
|-------|------|----------|
| plane | 1 | Visible in Move mode |
| arrow | 0 | Not visible |
| dial | 3 | Not visible in Rotate mode (toolbar shows Rotate) |
| scale_box | 5 | Likely not visible |

All styles share `TransformGizmoOverlay::recordDraw` → `transform_gizmo.slang` → `ScreenOverlayPass` with `depth_test=false`. Plane uses 6 vertices; dial uses 1728. The failure pattern points to non-plane procedural geometry or insufficient thickness—not rotate controller logic (pick/drag already implemented).

Toolbar sync confirms engine mode is `rotate` when UI highlights Rotate.

## Goals / Non-Goals

**Goals:**

- Rotate mode displays three colored torus dials at the selection pivot from typical camera angles.
- Move mode displays axis arrows in addition to plane handles.
- Scale mode displays per-axis and uniform box handles.
- Gizmo mode / space changes always refresh the viewport offscreen texture.

**Non-Goals:**

- Screen-space constant-width gizmos (Blender parity stretch goal).
- Line-overlay pass migration for dials.
- Changing rotation/scale math or pick tolerances unless needed after visuals work.

## Decisions

### 1. Treat as shared non-plane render fix, not rotate-only

**Decision:** Fix `vsDial`, `vsArrow`, and `vsScaleBox` together in one pass, using plane (working) as the reference path.

**Rationale:** Same pipeline, same `projectLocal`, same failure class. Fixing only dials may leave arrows/scale broken.

### 2. Thicken procedural geometry first

**Decision:** Increase dial `tubeR`, arrow `stemR`/`coneR`, and scale box half-extent floors in `transform_gizmo.slang`. Add style-specific `lineWidthScale()` minimums for dial (style 3/4) matching arrow (style 0).

**Alternatives:**

- Move dials to line overlay pass: larger refactor; defer unless shader fix fails.
- Billboard impostors: over-engineered for this bugfix.

### 3. Shader robustness for dial

**Decision:** Audit `vsDial` vertex generation for degenerate triangles; ensure inactive ghost segments (style 4) do not affect style 3; clip-space `w` sanity—discard vertices with `w <= 0` in vertex stage if needed.

**Investigation:** Compare `emitV` output for plane vs dial at same pivot matrix.

### 4. Pipeline / depth verification

**Decision:** Confirm `enable_depth_test=false` and `enable_depth_write=false` on the transform gizmo pipeline at runtime (RenderDoc). ScreenOverlayPass loads scene depth; if depth test were accidentally enabled, center-origin arrows/dials would fail while offset planes pass.

**Mitigation:** If depth leak found, set `CompareOp::Always` or explicitly disable depth attachment influence for gizmo subpass.

### 5. Viewport invalidation

**Decision:** Audit `RenderSystem::renderViewport` skip paths when only `m_viewport_render_generation` changes (gizmo mode). `requestViewportRedraw` already sets `m_force_viewport_render`; verify zero-copy `skip_camera_only_zero_copy` cannot drop overlay updates after mode switch.

**Optional:** Mark Slint viewport dirty when `setTransformGizmoMode` fires.

### 6. Debug spike (implementation phase)

**Decision:** During apply, use RenderDoc or temporary `LOG_INFO` in `recordGizmoDraw` rotate branch counting draws and `group_scale` to confirm CPU path runs before GPU fix.

## Risks / Trade-offs

- **[Risk] Thicker dials look less Blender-like** → Tune `tubeR` and `majorR` after visible; prefer readable over exact match.
- **[Risk] Root cause is not thickness (e.g. NaN basis)** → Debug spike catches; `buildGizmoBasis` already works for planes on same selection.
- **[Risk] Viewport perf skips leave stale texture** → Force composite on gizmo state change.

## Migration Plan

Follow-up PR after `fix-transform-gizmo`. No data migration. Validate: select entity → R → see three rings; G → planes + arrows; S → boxes.

## Open Questions

- Should dials use higher alpha (1.0) than planes for readability? **Default:** keep theme alpha unless still invisible after thicken.
