## 1. Transform theme parity

- [x] 1.1 Blender default `TH_AXIS_*` in `coordinate_system.h`
- [x] 1.2 `TH_GIZMO_VIEW_ALIGN` white in `transform_gizmo_types.cpp`
- [x] 1.3 Theme tests in `transform_gizmo_hover_test.cpp`

## 2. Navigate metrics

- [x] 2.1 `navigate_gizmo_style.h` diameter 80 / offset 10
- [x] 2.2 `NavigateGizmoMetrics` arm 0.80, ball 0.20
- [x] 2.3 `navigate_gizmo_style_test.cpp` + CMake

## 3. Navigate depth colors

- [x] 3.1 `navigate_gizmo_style.cpp` fade helpers
- [x] 3.2 `navigate_gizmo.slang` depth interp + `viewBgColor`
- [x] 3.3 Overlay UBO uses theme axis RGB w=1.0

## 4. Navigate hover

- [x] 4.1 `updateHoverFromPointer` / `clearHover` on overlay
- [x] 4.2 `RenderSystem` `MouseMoved` wiring
- [x] 4.3 Highlight backdrop + letter highlight in shader

## 5. Docs and validation

- [x] 5.1 `render-pipeline.md` navigate style section
- [x] 5.2 `openspec validate gizmo-blender-style-parity`
- [ ] 5.3 Manual: side-by-side Blender navigate + transform colors
