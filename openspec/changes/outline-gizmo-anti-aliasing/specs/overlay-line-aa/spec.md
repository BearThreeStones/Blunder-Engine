## ADDED Requirements

### Requirement: Overlay line anti-aliasing composite

When `BLUNDER_EDITOR_OVERLAY_AA` is enabled, the editor SHALL composite overlay lines using a fullscreen pass that decodes per-pixel line data (`pack_line_data` encoding), applies Blender-equivalent `line_coverage` smoothstep with `LINE_SMOOTH_START` / `LINE_SMOOTH_END` derived from `DISC_RADIUS`, and blends cross-neighbor pixels (±1 texel horizontal/vertical) with depth-aware alpha-over/under. A CPU-testable mirror SHALL exist in `overlay_line_aa.cpp`.

Reference: `E:/Dev/Blender/source/blender/draw/engines/overlay/shaders/overlay_antialiasing.bsl.hh`.

#### Scenario: Line data decode

- **WHEN** packed line data is `(sin_theta * 0.5 + 0.5, dist * 0.4 + 0.5)` at a line center pixel
- **THEN** `decodeLineData` SHALL recover `dist ≈ 0` at the stroke center

#### Scenario: Smooth line coverage at center

- **WHEN** `lineCoverage` is evaluated at distance 0 with smooth mode enabled
- **THEN** coverage SHALL be approximately 1.0

#### Scenario: Grid lines benefit from composite AA

- **WHEN** overlay AA is enabled and grid lines are drawn through `OverlayLinePass` with valid line MRT data
- **THEN** grid line edges SHALL show partial-alpha pixels along the stroke

#### Scenario: Disabled overlay AA preserves v1 behavior

- **WHEN** `BLUNDER_EDITOR_OVERLAY_AA` is unset or zero
- **THEN** the overlay AA pass SHALL NOT alter the main color buffer beyond existing v1 behavior
