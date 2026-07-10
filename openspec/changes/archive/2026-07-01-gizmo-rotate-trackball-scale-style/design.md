## Context

Completed gizmo work:

| Change | Delivers |
|--------|----------|
| `gizmo-blender-style` | Cone arrows, center disc, thin dials, scale cubes |
| `rotate-gizmo-dial-clip` | Axis dial semicircle clip (CLIP flag) |
| `gizmo-outer-ring` | `rot_c` white outer ring, `scale_c_outer` annulus |

Blender reference (user codemap + `transform_gizmo_3d.cc` / `dial3d_gizmo.cc`):

```
Rotate draw order (back → front):
  ROT_T  trackball  FILL, alpha×0.05, view-aligned
  ROT_X/Y/Z  dials  CLIP, line_width=3, axis colors
  ROT_C  outer ring  NOP, scale 1.2, white

Scale layout:
  SCALE_X/Y/Z  BOX + STEM at axis end
  SCALE_XY/YZ/ZX  PLANE corners, length 0.7
  SCALE_C  ANNULUS (done as scale_c_outer)
  center uniform cube (trans_c / scale_box center)
```

Current engine scale mode: 3× `scale_box` cubes + center cube + annulus. No plane handles. No box+stem.

## Goals / Non-Goals

**Goals:**

- Add `rot_t` trackball filled disc overlay (visual)
- Restyle scale axis handles as box+stem
- Add scale plane handles XY/YZ/ZX with pick + drag
- Preserve completed outer ring / dial clip / annulus behavior

**Non-Goals:**

- Trackball rotation drag (`TRANSFORM_OT_trackball` equivalent)
- `WM_GIZMO_DRAW_VALUE` angle numeric display
- Hover highlight (`WM_GIZMO_DRAW_HOVER`)
- Ghost arc helplines (partial ghost arc exists for axis dial drag)

## Decisions

### 1. New axis `rot_t` and draw style `dial_fill`

**Decision:** Add `ManipulatorAxis::rot_t` and `GizmoDrawStyle::dial_fill`:

- Reuse dial ring vertex generation with `majorR = 1.0`, filled disk (not wire tube)
- Fragment shader: flat color, alpha = `k_gizmo_color_alpha * 0.05`
- Matrix: `gizmoViewAlignedCenterMatrix`, scale basis 1.0 (same size as outer ring mesh radius)
- Draw first in rotate branch (before `rot_c` outer ring)

**Alternative:** Reuse `dial_wire` with high alpha — rejected; Blender uses filled disc.

### 2. Rotate draw order (Blender parity)

**Decision:**

1. `rot_t` trackball (filled, faint)
2. `rot_c` outer ring (wire, white)
3. `rot_x/y/z` axis dials (colored, clipped)
4. `dial_ghost` on drag (unchanged)

### 3. Scale box+stem geometry

**Decision:** Add `GizmoDrawStyle::scale_stem_box`:

- Stem: thin quad billboards or line along +Z local (0.2 → 0.8 offset), reuse arrow stem path shortened
- Box: reuse `vsScaleBox` at `k_mesh_scale_box_center_offset` (0.8)
- Single draw call per axis combining stem + box vertices (~42 verts)
- Replace `drawScaleHandle` for `trans_x/y/z` to use new style

**Metrics:** Stem start = 0.2 (match plane offset), box at 0.8 (existing).

### 4. Scale plane handles

**Decision:** Reuse translate plane geometry and pick for scale:

- Add `ManipulatorAxis::scale_xy`, `scale_yz`, `scale_zx` OR reuse `trans_xy/yz/zx` in scale mode draw/pick branches only
- Prefer **reuse trans_xy/yz/zx** in scale overlay/pick when mode is scale — avoids enum explosion
- Plane half-extent × `k_scale_plane_length_factor` (0.7)
- Colors from plane axis mapping (already in `themeAxisRgb`)

### 5. Pick and drag for scale planes

**Decision:** Extend `pickScaleGizmoHandle` to call plane pick logic (same as translate) with scale group scale. Extend `applyScaleDrag` masks — already mirror translation masks via `manipulatorTranslationMask` pattern for scale.

## Risks / Trade-offs

- **[Risk] Draw budget** — rotate +1 trackball, scale +3 planes → worst case rotate 6, scale 8; increase `k_max_gizmo_draws_per_frame` to 16 if needed
- **[Risk] scale_stem_box vertex budget** — keep per-axis one draw
- **[Risk] Plane handles overlap axis boxes** — use Blender offsets (planes at 0.2 corner, boxes at 0.8)

## Migration Plan

After `gizmo-outer-ring` archived. Manual QA: rotate shows faint disc; scale shows stems+boxes+planes+annulus.

## Open Questions

- Separate enum entries for scale planes vs reuse trans_xy in scale mode? **Default:** reuse in scale code paths only.
