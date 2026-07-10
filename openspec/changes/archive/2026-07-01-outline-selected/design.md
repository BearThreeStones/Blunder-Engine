## Context

Blunder's editor tracks a single selected entity via `EditorSelectionSystem` and positions the transform gizmo from `OverlayState::selection_world`, but the 3D viewport does not draw a selection outline on the mesh itself.

The render pipeline (`docs/agents/render-pipeline.md`) already sequences overlay work after the forward pass: scene overlays inside the forward pass, then `draw_overlay_lines` (MRT), `draw_overlay_aa`, SSAO, and `draw_screen_overlays`. `WireframeOverlay` exists as a stub and is **not** the right home for silhouette outlines—Blender keeps these separate (`Outline` vs wireframe).

Blender's Overlay engine implements **Outline Selected** as two GPU passes:

1. **Prepass** — render selected object surface geometry to a `R16_UINT` object-ID texture (+ depth).
2. **Resolve** — fullscreen edge detection on ID discontinuities, composite outline color with depth-based occlusion.

This change ports that pattern into Blunder's Vulkan/Slang overlay stack.

**User decisions (locked for v1):**

- Surface prepass (silhouette), not X-Ray edge-detection geometry.
- **Default on** when an entity is selected (no UI toggle, no env-var gate).
- **Hardcoded Blender orange** for selected outline (`theme.colors.object_select` equivalent).
- **Opaque and transparent** selected meshes both participate.

## Goals / Non-Goals

**Goals:**

- Visible orange silhouette outline around the selected entity's mesh in the editor viewport.
- Two-pass GPU implementation (prepass + resolve) aligned with Blender's Overlay `Outline` class.
- Integrate into existing `OverlaySystem` phase ordering without modifying PBR forward shaders.
- Support both opaque and blend (`cgltf_alpha_mode_blend`) mesh renderers on the selected entity.
- Depth occlusion: outline behind other geometry renders at reduced alpha (Blender `alpha_occlu`).

**Non-Goals:**

- Multi-selection or per-object ID tables (single selection → constant ID `1`).
- Transform-drag color variant (`color_id = 0`, transform orange)—follow-up.
- X-Ray mode edge-detection line geometry (`DRW_cache_mesh_edge_detection_get` equivalent).
- UI toggle (`V3D_SELECT_OUTLINE`), theme system, outline width preferences.
- GPU selection / picking pass (outline always eligible when selected).
- Thick-outline expansion (HiDPI / width > 2 px)—follow-up.
- `WireframeOverlay` mesh-edge display (separate future work).

## Decisions

### 1. New `OutlineOverlay` + `OutlineTargets` (not `WireframeOverlay`)

**Decision:** Add `OutlineOverlay` and `OutlineTargets` under `engine/src/runtime/function/render/overlay/`, orchestrated by `OverlaySystem`. Leave `WireframeOverlay` stub untouched.

**Rationale:** Silhouette outline (ID pass + resolve) and triangle wireframe (line topology) are distinct Blender overlays with different shaders and enable rules.

**Alternatives considered:**

- Extend `WireframeOverlay` — rejected; conflates two features.
- Inline passes in `RenderSystem` — rejected; breaks overlay layering convention.

### 2. Pipeline insertion: after forward pass, before overlay lines

**Decision:** Call `OutlineOverlay::draw` from `RenderSystem::tickVulkan` immediately after `ForwardRenderPath::renderFrame` returns (same command buffer), before `draw_overlay_lines`.

```
Forward (opaque + transparent + scene overlays)
  → Outline prepass (ID + depth RTs)
  → Outline resolve (composite to main color, LOAD pass)
  → draw_overlay_lines / AA / SSAO / screen overlays
```

**Rationale:** Resolve needs finalized scene depth from the forward pass. Running before line overlays avoids outline fighting with grid AA MRT.

### 3. Surface prepass geometry

**Decision:** Draw the selected entity's `GpuMesh` with triangle-list surface indices (same buffers as forward pass). Edge detection happens in the resolve shader via neighbor ID comparison—not via explicit edge geometry.

**Rationale:** Matches Blender's non-X-Ray path; lowest implementation cost. `GpuMesh` has no edge-detection cache.

### 4. Single-selection ID encoding

**Decision:** Prepass fragment shader writes constant `ob_id = 1` (background = `0`). No 14-bit object packing in v1.

**Rationale:** `EditorSelectionSystem` is single-select. Resolve only needs foreground vs background discontinuities.

### 5. Transparent mesh prepass

**Decision:** Include blend-mode mesh renderers in the prepass. Prepass pipeline: depth test ON, depth write ON, **no blending**, color attachment writes uint ID only. Draw selected mesh regardless of `cgltf_alpha_mode`.

**Rationale:** User requested transparent support. Silhouette follows the mesh surface hull; alpha cutout meshes use the same surface path as forward opaque.

**Alternatives considered:**

- Skip blend meshes — rejected per user choice.
- Depth write OFF for transparent — rejected; breaks occlusion comparison for outline behind glass.

### 6. Render targets and formats

**Decision:** `OutlineTargets` owns:

| Attachment | Format | Purpose |
|------------|--------|---------|
| `object_id` | `VK_FORMAT_R16_UINT` (fallback `R32_UINT` if unsupported) | Packed ID (constant `1` in v1) |
| `outline_depth` | `VK_FORMAT_D32_SFLOAT` | Depth at outline surface for occlusion test |

Resolve pass samples `object_id`, `outline_depth`, and `OffscreenRenderTarget::getDepthImageView()` (scene depth).

**Rationale:** Mirrors Blender's `object_id_tx_` + `outline_depth_tx` + `scene_depth_tx`. Independent outline depth avoids Z-fighting with the main color pass when re-rasterizing the same mesh.

### 7. Resolve shader behavior

**Decision:** Port core logic from Blender `overlay_outline_detect_frag.glsl`:

- Sample 4-neighbor IDs at ±1 pixel; edge when neighbor ID ≠ center ID (and center ≠ 0).
- Map non-zero ID to hardcoded orange `float4(0.960784f, 0.439216f, 0.066667f, 1.0f)` (~ `#F57011`, Blender default object select).
- Compare `outline_depth` vs `scene_depth` with epsilon `3.0f / 8388608.0f`; occluded alpha = `0.35`.
- Output alpha capped at `254/255` to preserve edge AA headroom (Blender convention).
- Composite via alpha blend onto main color (LOAD render pass), same pattern as `OverlayAntiAliasing`.

**Alternatives considered:**

- Always-opaque outline — rejected; occlusion feedback is essential for readability.

### 8. Enable conditions

**Decision:**

```cpp
enabled_ = editor_selection->hasSelection()
        && selected_entity_has_mesh_renderer;
```

No env var, no UI flag. Disabled when nothing selected.

### 9. Selection sync path

**Decision:** In `OutlineOverlay::begin_sync`, resolve selected `EntityId` → `MeshRendererComponent` + `GpuMesh` + world matrix via `SceneInstance` (same data path as `syncSceneToRender`, scoped to one entity). Cache draw payload on `OutlineOverlay` for the prepass.

**Rationale:** Avoids tagging every `OpaqueMeshDraw` in `scene_render_bridge` for v1.

## Risks / Trade-offs

- **[Risk] `R16_UINT` color attachment unsupported on some GPUs** → Probe `vkGetPhysicalDeviceFormatProperties`; fall back to `R32_UINT`. **Mitigation:** Check at `OutlineTargets::initialize`.
- **[Risk] Transparent prepass silhouette differs from perceived transparency** → Outline follows full surface hull, not only visible pixels. **Mitigation:** Acceptable for v1; matches Blender surface prepass for non-X-Ray.
- **[Risk] Extra GPU cost (2 passes/frame)** → Only one mesh drawn in prepass; resolve is fullscreen. **Mitigation:** Negligible vs forward PBR; benefits from `BLUNDER_EDITOR_RENDER_SCALE`.
- **[Risk] Depth fighting on resolve occlusion** → Blender epsilon mitigates. **Mitigation:** Use same epsilon; tune if artifacts appear.
- **[Risk] Double-buffer / layout barriers** → Outline RTs must barrier to `SHADER_READ_ONLY` before resolve; scene depth must be `DEPTH_STENCIL_READ_ONLY_OPTIMAL`. **Mitigation:** Follow `OverlayAntiAliasing` / SSAO barrier patterns.

## Migration Plan

- Purely additive rendering feature; no asset or scene format changes.
- Default-on: visible immediately when selecting any mesh entity in the editor.
- Rollback: set `OutlineOverlay::begin_sync` to `enabled_ = false` or skip draw calls in `RenderSystem::tickVulkan`.

## Open Questions

- None blocking v1. Follow-ups: transform-drag color, thick outlines, multi-select IDs, UI toggle, X-Ray edge geometry.
