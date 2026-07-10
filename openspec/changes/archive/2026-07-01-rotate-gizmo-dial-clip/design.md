## Context

Rotate dials are drawn by `TransformGizmoOverlay::drawRotationDial` using `GizmoDrawStyle::dial` (full ring, 48 segments) and `GizmoDrawStyle::dial_ghost` (angular arc during drag via `dialSegmentActive` in vertex shader). The uniform struct `TransformGizmoUniformData` packs style/alpha/line-width/arc params in `params` vec4; there is no clip plane today.

Blender reference flow:

```
init:     rot axes → ED_GIZMO_DIAL_DRAW_FLAG_CLIP
draw:     use_clip = !modal && CLIP
plane:    normal = viewinv[2] (toward camera), through pivot + bias
shader:   GPU_SHADER_*_CLIPPED_* + immUniform4fv("ClipPlane")
```

Our engine uses a single procedural shader (`transform_gizmo.slang`) and `TransformGizmoController::isDragging()` as the modal equivalent.

## Goals / Non-Goals

**Goals:**

- Idle rotate dials: camera-facing semicircle via fragment clip plane.
- Active rotation drag: disable clip → full ring (all three dials unclipped while dragging).
- Clip plane: view direction from pivot to camera, through pivot, with pixel-scaled bias.
- Ghost arc (`dial_ghost`): no semicircle clip (angular arc only).

**Non-Goals:**

- Rotation angle numeric HUD (`WM_GIZMO_DRAW_VALUE`).
- Trackball / view-relative rotation ring.
- Screen-space constant-width dial lines.
- Changing rotation math or ghost arc angular logic.

## Decisions

### 1. Fragment shader clip plane (Blender-equivalent)

**Decision:** Add `float4 clip_plane` (xyz = world normal toward camera, w = plane constant) and `float clip_enabled` to the gizmo uniform. In `fragmentMain`, when `clip_enabled > 0.5`, discard if `dot(normalize(clip_plane.xyz), worldPos) + clip_plane.w < 0`.

World position recovered from interpolated data or recomputed — pass world position from vertex shader via new `[[vk::location(2)]] float3 worldPos`.

**Alternatives:**

- Vertex-stage segment culling by angle: harder to match Blender plane exactly; defer.
- CPU-side reduced arc draw: changes pick topology; more invasive.

### 2. Clip plane construction (matches Blender 3a–3c)

**Decision:** Add `buildDialClipPlane(pivot, camera_position, pixel_size)` in `gizmo_math.cpp`:

```cpp
glm::vec3 n = normalize(camera_position - pivot);  // toward camera
float d = -dot(n, pivot);
d += k_dial_clip_bias * pixel_size;  // Blender DIAL_CLIP_BIAS ≈ 0.02
return glm::vec4(n, d);
```

Use `computeView3dPixelSizeNoUiScale` at pivot for bias scaling (same helper as gizmo group scale).

### 3. Enable/disable rules (matches Blender 2a–2b)

**Decision:**

| Draw call | `clip_enabled` |
|-----------|----------------|
| `GizmoDrawStyle::dial`, not dragging | `1.0` |
| `GizmoDrawStyle::dial`, dragging | `0.0` |
| `GizmoDrawStyle::dial_ghost` | `0.0` |

When `isDragging()` and axis is rotation, all three base dials draw unclipped (Blender modal disables clip globally for dial draw path).

### 4. Uniform layout extension

**Decision:** Extend `TransformGizmoUniformData` / shader struct:

```cpp
glm::vec4 clip_plane{0, 0, 1, 0};
float clip_enabled{0.0f};
float _pad2[3]{};
```

Non-dial styles pass `clip_enabled = 0`. Uniform buffer size increases; all draw slots already allocated — no descriptor change beyond struct size.

### 5. Pick consistency (optional in first pass)

**Decision:** Defer pick-plane clip test unless manual QA finds clicks hitting invisible back half. Full-ring pick during idle is acceptable short-term because ray-plane ring test intersects front arc first in most views.

If needed follow-up: reject pick hits where `dot(n, hit) + d < 0`.

## Risks / Trade-offs

- **[Risk] Uniform struct size / alignment mismatch** → Update C++ and slang together; rebuild shader.
- **[Risk] Edge flicker when view parallel to dial plane** → Bias term mitigates; tune `k_dial_clip_bias`.
- **[Risk] clip_enabled conflates with other styles** → Only set on dial draws in overlay.

## Migration Plan

Single PR. Manual validation:

1. Rotate mode idle → three semicircles facing camera.
2. Orbit camera → semicircles flip to stay facing camera.
3. Start drag → full rings visible; ghost arc shows delta.
4. Release → semicircles return.

Rollback: revert shader + uniform + overlay changes.

## Open Questions

- Clip only the active dial during drag vs all three? **Default:** all three unclipped while dragging (Blender global `!is_modal`).
