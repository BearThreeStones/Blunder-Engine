## Why

Selection outlines, transform gizmo handles, and overlay lines (grid, future axes/wireframe) show visible aliasing at 1× viewport resolution. v1 landed 8-neighbor outline kernel tuning and transform gizmo `strokeCoord`/`fwidth` polyline AA, but quality still falls short of Blender Overlay Engine parity. Blender (`E:/Dev/Blender`) uses three shader-based AA layers without MSAA: polyline intrinsic AA (`GPU_SHADER_3D_POLYLINE_*`), line MRT + fullscreen `overlay_antialiasing` composite, and (optionally) directional outline detect with `pack_line_data`.

## What Changes

**v1 (implemented):**

- `outline_aa.cpp` + widened kernel in `outline_resolve.slang` (`kOutlineEdgeSmoothMin/Max` 0.25 / 2.5).
- Transform gizmo polyline `strokeCoord` + fragment softening in `transform_gizmo.slang`.
- CPU mirrors and unit tests for both paths.

**v2 — Blender parity (this change continuation):**

- Implement Blender-style overlay line AA composite in `overlay_aa.slang` (port `overlay_antialiasing.bsl.hh` cross-neighbor `line_coverage`).
- CPU mirror `overlay_line_aa.cpp` + `packLineData` shared encoding (`overlay_common_lib.glsl`).
- Align transform gizmo polyline AA with Blender `smoothline` formula (`SMOOTH_WIDTH = 1`).
- Route grid lines through `OverlayLinePass` line MRT when `BLUNDER_EDITOR_OVERLAY_AA=1`.
- Draw transform gizmo arrow stems as polylines (Blender `arrow3d_gizmo.cc` pattern).

**Optional v3:** Directional outline detect (`overlay_outline_detect_frag.glsl`) — only if manual QA still flags diagonal outline quality.

Out of scope: viewport MSAA; compositor SMAA; navigate gizmo (already has disc smoothstep); user AA strength preference.

## Capabilities

### New Capabilities

- `overlay-line-aa`: Line MRT decode + fullscreen cross-neighbor anti-aliasing composite (Blender `overlay_antialiasing`).

### Modified Capabilities

- `outline-smooth-resolve`: v1 kernel + CPU mirror (done); optional v3 directional detect deferred.
- `transform-gizmo`: v1 polyline AA (done); v2 Blender `smoothline` formula + arrow stem polylines.

## Impact

- `engine/shaders/overlay_aa.slang`, `engine/shaders/overlay_line_pack.slang`, `engine/shaders/grid_line.slang`
- `engine/shaders/transform_gizmo.slang`, `engine/shaders/outline_resolve.slang`
- `engine/src/runtime/function/render/overlay/outline_aa.{h,cpp}` (v1)
- `engine/src/runtime/function/render/overlay/overlay_line_aa.{h,cpp}` (v2)
- `engine/src/runtime/function/render/gizmo/gizmo_polyline_aa.{h,cpp}`
- `engine/src/runtime/function/render/overlay/grid_overlay.cpp`
- `engine/src/tests/outline_aa_test.cpp`, `transform_gizmo_aa_test.cpp`, `overlay_line_aa_test.cpp`, `gizmo_polyline_blender_aa_test.cpp`
- `docs/agents/render-pipeline.md`
- Reference: `E:/Dev/Blender/source/blender/draw/engines/overlay/shaders/overlay_antialiasing.bsl.hh`
