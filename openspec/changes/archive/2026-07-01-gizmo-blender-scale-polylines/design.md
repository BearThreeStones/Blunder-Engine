## Context

Current scale (`gizmo_math.cpp`):

```cpp
// Ortho:  ortho_size / vp
// Persp:  clip.w * 2 / vp
group_scale = gizmo_size * pixel_size;
```

Blender (`wm_gizmo.cc`):

```
scale_final = scale_basis × UI_SCALE_FAC × U.gizmo_size
            × ED_view3d_pixel_size_no_ui_scale(rv3d, world_pos)

ED_view3d_pixel_size = is_persp ? pixsize × depth : pixsize
pixsize = ortho_scale / viewfac          (ortho)
        = (sensor × clip_start) / lens / viewfac   (persp)
```

Blender also stores `rv3d->pixsize` from projection matrix row lengths (`view3d_draw.cc`):

```
len_px = 2 / sqrt(min(|persmat[0]|², |persmat[1]|²))
pixsize = len_px / max(winx, winy)
```

Our grid overlay already approximates world-per-pixel differently (`grid_overlay.cpp`), contributing to scene vs gizmo mismatch.

Rotation dials use `vsDialRing` tube quads scaled by `group_scale × kMeshDialTubeHalfWidth × lineWidthScale` — visually thicker than Blender polylines.

## Goals / Non-Goals

**Goals:**

- Match Blender gizmo screen size in perspective and orthographic modes at the same orbit distance / ortho_size.
- Render rotation axis dials, outer white ring, and annulus outer wire as screen-space polylines.
- Blender line widths: rot X/Y/Z = 3px, rot_c / dial_wire = 2px, planes = 1px.
- Reduce "gizmo eats half the viewport" in small docked windows via optional size cap.

**Non-Goals:**

- Full Blender camera sensor/lens model (use projection-derived `pixsize` instead).
- Polyline translate arrows or scale stem/box handles.
- Preferences UI for gizmo size.

## Decisions

### 1. Shared `computeViewportPixsize()`

**Decision:** Add function mirroring Blender `rv3d->pixsize`:

```cpp
float computeViewportPixsize(const glm::mat4& view_projection,
                             uint32_t viewport_width,
                             uint32_t viewport_height,
                             bool is_perspective);
```

Implementation:
- Extract perspective-scaled rows from `view_projection` (or pass `projection * view` as today).
- `len_px = 2 / sqrt(min(len2(row0.xyz), len2(row1.xyz)))`
- `viewfac = max(viewport_width, viewport_height)` (match Blender `len_sc`).
- `pixsize = len_px / viewfac`

For orthographic-only fast path when `projection[3][3] != 0`:
- `pixsize = ortho_size / viewfac` (our `ortho_size` is full visible height = Blender `ortho_scale`).

**Alternative:** Keep separate ortho/persp branches — rejected; caused Persp/Iso mismatch.

### 2. `ED_view3d_pixel_size` equivalent

**Decision:**

```cpp
float computeGizmoPixelSizeNoUiScale(const TransformGizmoScaleContext& ctx) {
  const float pixsize = computeViewportPixsize(...);
  if (ctx.is_perspective) {
    const float depth = computePivotDepth(ctx); // |clip.w| from VP × pivot
    return pixsize * depth;
  }
  return pixsize;
}

float computeGizmoGroupScale(const TransformGizmoScaleContext& ctx) {
  float effective_gizmo_size = ctx.gizmo_size;
  // optional cap — see §5
  return ctx.ui_scale * effective_gizmo_size
       * computeGizmoPixelSizeNoUiScale(ctx);
}
```

Per-handle: `handle_scale = group_scale * gizmoScaleBasisForAxis(axis)` unchanged.

### 3. Screen-space polyline dials

**Decision:** Add `GizmoDrawStyle::polyline_wire` (or reuse dial styles with flag) in `transform_gizmo.slang`:

- Vertex stage: emit circle/annulus centerline points in local XY (radius = mesh major radius 0.82 / 1.0).
- Expand to screen-space quads in clip space using `lineWidthPixels` uniform (Blender `GPU_SHADER_3D_POLYLINE` approach: extrude in screen plane).
- Fragment: flat color + clip plane discard (reuse existing `clip_enabled` / `clip_plane` for axis dials).

Styles migrated to polyline path:
- `dial` (axis rotation rings)
- `dial_wire` (rot_c outer ring)
- Annulus **outer** ring only; inner ring can remain tube or second polyline at smaller radius.

Styles staying on geometry path:
- `dial_fill` (trackball)
- `dial_ghost` (filled arc)
- `arrow`, `plane`, `scale_stem_box`, `scale_box`, `center`

**Line width uniform:** pass Blender px values directly (`3.0f`, `2.0f`) — not multiplied by `group_scale`.

### 4. Mesh radius vs scale

**Decision:** With polylines, dial **radius** still scales with `gizmo_world` matrix (via `group_scale`); only **stroke width** is screen-constant. Matches Blender: `imm_draw_circle_wire_3d` at radius 1.0 with separate `lineWidth` uniform.

Tune `k_mesh_dial_major_radius` back to `1.0` if currently `0.82` for tube inflation compensation.

### 5. Small viewport cap

**Decision:** After computing target screen diameter `≈ gizmo_size` px, clamp:

```cpp
effective_gizmo_size = min(gizmo_size,
    min(vp_w, vp_h) * k_viewport_gizmo_size_factor); // 0.35, match navigate gizmo
```

Only affects `gizmo_size` multiplier, not `pixsize` math.

### 6. Pick adjustment

**Decision:** `pickDialRing` uses world major radius from `handle_scale × k_mesh_dial_major_radius` plus pick slop derived from **3px** screen width converted to world units via `pixel_size`.

## Risks / Trade-offs

- **[Risk] Polyline clip plane** — screen extrusion + clip plane interaction needs validation at glancing angles.
- **[Risk] Vulkan wide lines** — if hardware line width unsupported, must use quad expansion (planned in shader).
- **[Risk] Regression on scale/translate** — scope limited to dial wire styles; arrow/box paths untouched.
- **[Trade-off] Shared pixsize with grid** — defer grid refactor unless trivial reuse.

## Migration Plan

1. Land scale formula fix first → verify Persp/Iso toggle size parity.
2. Land polyline dials → verify line width and clip.
3. Manual QA: small viewport perspective, large viewport, rotate/scale/translate unchanged.

## Open Questions

- Use `max(w,h)` or `min(w,h)` for viewfac? **Default:** Blender `max(winx,winy)`.
- Annulus inner ring polyline or keep tube? **Default:** outer polyline, inner polyline at `radius / 6`.
