## ADDED Requirements

### Requirement: Transform gizmo polyline anti-aliasing

Screen-space polyline gizmo styles (rotation axis dials, view-aligned outer ring, scale annulus wire, ghost arc) SHALL render with edge softening using a `strokeCoord` varying and fragment alpha falloff matching Blender smooth polyline overlay quality (`GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR`). Solid mesh styles (arrows, planes, scale boxes) are out of scope for polyline AA except arrow **stems** (see arrow stem requirement below).

#### Scenario: Rotate dial edge softness

- **WHEN** rotate mode is active and axis rotation dials are visible
- **THEN** dial ring edges SHALL show partial-alpha pixels along the stroke (not a hard 1-pixel aliased edge only)

#### Scenario: Scale annulus edge softness

- **WHEN** scale mode is active and the outer annulus wire is visible
- **THEN** annulus edges SHALL show partial-alpha pixels along the stroke

#### Scenario: Polyline AA does not regress clip or width

- **WHEN** rotate mode is idle with semicircle clip active
- **THEN** polyline anti-aliasing SHALL apply only to visible (non-discarded) polyline fragments
- **AND** dial stroke width in screen pixels remains approximately 3px for axis dials and 2px for the outer ring

#### Scenario: CPU regression mirror

- **WHEN** `transform_gizmo_aa_test` runs
- **THEN** `polylineStrokeAlpha` at stroke center SHALL return full alpha and at stroke edge SHALL return zero alpha

## MODIFIED Requirements

### Requirement: Transform gizmo polyline anti-aliasing (Blender formula)

v2 SHALL align polyline fragment alpha with Blender `gpu_shader_3D_polyline_frag.glsl`: `alpha *= clamp((lineWidth + kPolylineSmoothWidth) * 0.5 - abs(smoothline), 0, 1)` where `kPolylineSmoothWidth = 1.0`. CPU mirror `polylineStrokeAlphaBlender` SHALL be tested in `gizmo_polyline_blender_aa_test`.

#### Scenario: Blender center and edge alpha

- **WHEN** `polylineStrokeAlphaBlender(0, lineWidth, true)` is evaluated
- **THEN** alpha SHALL be 1.0
- **WHEN** `polylineStrokeAlphaBlender` is evaluated at the outer edge (`abs(smoothline)` equals half expanded width)
- **THEN** alpha SHALL be 0.0

## ADDED Requirements

### Requirement: Transform gizmo arrow stem polylines

Translate-mode arrow **stems** SHALL be drawn as screen-space polylines (not filled quads), with line width approximately one pixel (`line_width_px ≈ pixelsize`), matching Blender `arrow3d_gizmo.cc` use of `GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR`.

#### Scenario: Arrow stem edge softness

- **WHEN** translate mode is active and axis arrows are visible
- **THEN** arrow stem edges SHALL show partial-alpha pixels along the stroke
