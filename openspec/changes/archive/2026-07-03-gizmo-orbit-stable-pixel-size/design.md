## Context

Current scale path (`gizmo_math.cpp`, post `gizmo-blender-scale-polylines`):

```cpp
pixsize = len_px(VP columns 0/1) / max(w, h)
pixel_size = pixsize × |clip.w|     // persp only
group_scale = ui_scale × effective_gizmo_size × pixel_size
```

Previous stable path (pre-change):

```cpp
pixel_size = (2 / viewport_height) × |clip.w|   // persp; pixsize view-independent
```

Blender stores `rv3d->pixsize` from `persmat = winmat × viewmat` column lengths, but `ED_view3d_pixel_size_no_ui_scale` returns `mul_project_m4_v3_zfac(persmat, co) × pixsize`. For orbit invariance at a fixed world point on the view axis, **pixsize must not incorporate view rotation** while **zfac must use the full persmat**. Using VP columns for both numerator factors causes `pixsize` to vary ~30–50% during yaw/pitch while `clip.w` at the focal pivot stays constant — producing visible uniform gizmo breathing.

Numeric verification (orbit around focal, pivot at origin, distance 10): old formula constant; VP-column pixsize × constant clip.w varies significantly.

## Goals / Non-Goals

**Goals:**

- Restore orbit-stable `group_scale` when pivot is at viewport center and camera distance is fixed.
- Keep Persp/Iso screen-size parity and small-viewport cap from `gizmo-blender-scale-polylines`.
- Minimal API surface: extend `TransformGizmoScaleContext` with projection matrix for pixsize only.

**Non-Goals:**

- Change camera focal/orbit model (still virtual focal point).
- Revert polyline dials or line-width tuning.
- Match Blender `persmat` pixsize literally if it conflicts with orbit stability (prefer winmat-only pixsize).

## Decisions

### 1. Pixsize from projection only

**Decision:** Compute `pixsize` from `projection` matrix column lengths (same len_px formula), not `view_projection`.

```cpp
float computeViewportPixsize(const glm::mat4& projection, ...);
// persp: len_px from projection[0], projection[1] columns
// ortho: ortho_size / viewfac (unchanged)
```

**Alternative:** Revert entirely to `(2/vp_h) × clip.w` — rejected; breaks Persp/Iso alignment fixed in prior change.

**Alternative:** Keep VP pixsize, vary depth with Euclidean distance — rejected; diverges from Blender zfac model and still wrong for off-center pivots.

### 2. Depth unchanged

**Decision:** Keep `depth = |clip.w|` from `view_projection × pivot` (equivalent to Blender `mul_project_m4_v3_zfac` with GLM conventions).

### 3. Context wiring

**Decision:** Add `glm::mat4 projection` to `TransformGizmoScaleContext`. Overlay/controller set:

- `projection = state.projection`
- `view_projection = state.projection * state.view`

Pick and dial clip plane continue using `computeGizmoPixelSizeNoUiScale(ctx)` with the updated pixsize path.

### 4. Orbit regression guard

**Decision:** `#ifndef NDEBUG` helper (or extend `debugLogGizmoScaleParity`) that logs when `group_scale` at fixed pivot varies >5% across synthetic yaw/pitch samples at constant distance — optional manual QA checklist item.

## Risks / Trade-offs

- **[Risk] Persp/Iso ratio shifts slightly** — pixsize source change may move toggle parity outside [0.85, 1.15]; re-verify after fix, tune ortho path if needed.
- **[Risk] GLM vs Blender matrix layout** — column extraction must match existing len_px logic; use same `[0]`/`[1]` column access as today but on `projection` only.
- **[Trade-off] Off-center pivot** — orbit still changes camera-to-pivot distance; gizmo scale may drift when selection is not at focal (unchanged, not this change's scope).

## Migration Plan

1. Land pixsize split in `gizmo_math.cpp`.
2. Wire projection into overlay + controller contexts.
3. Build Debug; manual orbit at centered selection — gizmo diameter stable.
4. Re-run Persp/Iso toggle and small-viewport checks from `gizmo-blender-scale-polylines` QA.

## Open Questions

- None blocking. If Persp/Iso parity regresses, adjust ortho `pixsize` denominator only (not VP depth).
