## Context

Blender reference: `transform_gizmo_3d.cc` (`gizmo_get_axis_color`), `view3d_gizmo_navigate_type.cc` (depth fade), `view3d_gizmo_navigate.cc` (hover backdrop `color_hi` 0.5).

## Decisions

- **Theme tokens:** Hardcode Blender default theme RGB in `coordinate_system.h` until runtime theme editor exists.
- **Navigate colors:** CPU helpers in `navigate_gizmo_style.cpp`; shader duplicates fade formulas using `viewBgColor` uniform.
- **Navigate hover:** `NavigateGizmoOverlay::updateHoverFromPointer` mirrors transform gizmo pattern; does not set `event.handled`.
- **View background:** Default viewport gray `0.22` until scene clear color is plumbed to overlay.

## Out of scope

Slint pan/zoom/camera toolbar buttons; `U.gizmo_size` preference; transform line-width hover change.
