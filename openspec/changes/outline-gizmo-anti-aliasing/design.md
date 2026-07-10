## Context

Blender Overlay Engine (`E:/Dev/Blender`) anti-aliases editor overlays in three layers, all without MSAA:

1. **Polyline intrinsic AA** — `gpu_shader_3D_polyline_frag.glsl`: `fragColor.a *= clamp((lineWidth + SMOOTH_WIDTH) * 0.5 - abs(smoothline), 0, 1)` with `SMOOTH_WIDTH = 1`. Gizmo dials/arrows use `immUniform1f("lineWidth", …)`.
2. **Line MRT composite AA** — Overlays write color + `pack_line_data` (`overlay_common_lib.glsl`) to line target; fullscreen `overlay_antialiasing.bsl.hh` fetches cross neighbors, `line_coverage` smoothstep (`LINE_SMOOTH_START/END` from `DISC_RADIUS`), depth-aware `neighbor_blend`.
3. **Outline directional detect** — `overlay_outline_detect_frag.glsl` classifies 8-neighbor edge cases, packs directional line data for the same AA composite.

**Blunder v1 (done):** 8-neighbor outline kernel (`outline_aa.cpp`); transform gizmo `strokeCoord` + `fwidth` in `ScreenOverlayPass`. `overlay_aa.slang` binds `lineDataTexture` but only alpha-blends — does not run Blender line AA.

Implementation plans: `docs/superpowers/plans/2026-07-06-outline-gizmo-anti-aliasing.md` (v1), `docs/superpowers/plans/2026-07-06-blender-overlay-aa-parity.md` (v2).

## Goals / Non-Goals

**Goals:**

- v1: Smooth outline diagonals + gizmo polyline rings at 1× / `BLUNDER_EDITOR_RENDER_SCALE=0.75` (done).
- v2: Blender-parity overlay line AA composite; gizmo polyline formula alignment; grid via line MRT; arrow stems as polylines.
- CPU unit tests mirroring all AA math (TDD).

**Non-Goals:**

- MSAA / supersampled offscreen rendering.
- Compositor SMAA.
- Navigate gizmo changes (disc smoothstep sufficient).
- User preference toggle for AA strength.
- Axes/wireframe overlay implementation (stubs today) — only line MRT contract for future.

## Decisions

### 1. Shader-based AA only (no MSAA)

**Decision:** Keep 1× offscreen target; all AA via fragment math.

**Rationale:** MSAA complicates pick/outline ID buffers and Slint composite. Matches Blender overlay architecture.

### 2. Outline v1 — 8-neighbor kernel + CPU mirror (done)

**Decision:** `outlineEdgeCoverage()` with `kOutlineEdgeSmoothMin = 0.25`, `kOutlineEdgeSmoothMax = 2.5`.

**Rationale:** Testable without GPU; improves shallow diagonals vs old `smoothstep(0.5, 3.0)`.

**v3 alternative (deferred):** Port Blender `overlay_outline_detect` directional `pack_line_data` + route through overlay line AA.

### 3. Transform gizmo v1 — `strokeCoord` varying (done)

**Decision:** Polyline quads pass `strokeCoord` ∈ [-1, 1]; solids sentinel `> 1`.

### 4. Transform gizmo v2 — Blender `smoothline` formula

**Decision:** Replace `fwidth` path with Blender-equivalent: `alpha *= clamp((lineWidth + kPolylineSmoothWidth) * 0.5 - abs(smoothline), 0, 1)` where `kPolylineSmoothWidth = 1.0` (`SMOOTH_WIDTH`).

**Rationale:** Matches `gpu_shader_3D_polyline_frag.glsl`; deterministic across GPUs vs `fwidth`.

**Alternative rejected:** Keep `fwidth` only — driver-dependent, not Blender-identical.

### 5. Overlay line AA — port `overlay_antialiasing.bsl.hh`

**Decision:** Implement `decodeLineData`, `lineCoverage`, `neighborLineDist`, cross-neighbor blend in `overlay_aa.slang`; CPU mirror in `overlay_line_aa.cpp`. Gate with existing `BLUNDER_EDITOR_OVERLAY_AA=1`.

**Rationale:** C++ pipeline (`OverlayAntiAliasing`, `OverlayLineTargets`) already exists; shader was stub.

**Encoding:** Shared `packLineData` per `overlay_common_lib.glsl` (`sin_theta * 0.5 + 0.5`, `dist * 0.4 + 0.5`).

### 6. Grid → line MRT

**Decision:** Grid fine lines draw through `OverlayLinePass` using `grid_line.slang` MRT output when overlay AA enabled.

**Rationale:** Blender grid uses overlay line AA path; `grid_line.slang` already has lineData target.

### 7. Arrow stems as polylines

**Decision:** `vsArrow` stem segment uses `emitPolyline` with `line_width_px ≈ 1.0 * pixelsize` (Blender `U.pixelsize`).

**Rationale:** `arrow3d_gizmo.cc` draws stems via `GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR`, not filled quads.

### 8. TDD with CPU helpers

**Decision:** `outline_aa_test`, `transform_gizmo_aa_test`, `overlay_line_aa_test`, `gizmo_polyline_blender_aa_test`.

## Risks / Trade-offs

- **[Risk] Overlay AA perf** — Extra fullscreen pass + 4 neighbor fetches → Acceptable for editor; matches Blender.
- **[Risk] Grid dual path** — Grid may need both procedural fill (`grid.slang`) and line MRT → Start with line subset only.
- **[Risk] Outline v1 vs Blender detect** — Diagonal quality may still lag until optional v3.
- **[Risk] Formula swap regresses gizmo look** — Run hover + AA tests; manual R/S QA.

## Migration Plan

v1 ships default-on outline smooth resolve (no env gate). v2 overlay AA remains behind `BLUNDER_EDITOR_OVERLAY_AA=1` until manual QA passes, then consider default-on.

## Open Questions

- None blocking v2. Outline directional detect (v3) triggered only if QA rejects v1 kernel on diagonals.
