## 1. Uniform & math

- [x] 1.1 Add `clip_plane` and `clip_enabled` to `TransformGizmoUniformData` and `TransformGizmoUniform` in slang (keep alignment)
- [x] 1.2 Add `TransformGizmoMetrics::k_dial_clip_bias` (default 0.02, matches Blender)
- [x] 1.3 Implement `buildDialClipPlane(pivot, camera_position, pixel_size)` in `gizmo_math.cpp`

## 2. Shader clip pass

- [x] 2.1 Pass `worldPos` from vertex shader (`[[vk::location(2)]]`)
- [x] 2.2 In `fragmentMain`, discard fragments when `clip_enabled > 0.5` and behind `clip_plane`
- [x] 2.3 Verify non-dial styles unaffected (`clip_enabled = 0`)

## 3. Overlay wiring

- [x] 3.1 Extend `makeUniform` / `recordDraw` to accept clip plane + enable flag
- [x] 3.2 In `drawRotationDial`: enable clip for `dial` when `!m_controller.isDragging()`; disable for `dial_ghost` and during drag
- [x] 3.3 Build clip plane from `state.camera_position`, pivot, and pixel size each dial draw

## 4. Build and validation

- [x] 4.0 Fix std140 UBO layout: shader `float2 _pad_before_clip` to match C++ `float _pad[2]` (clip_plane @ offset 256)
- [x] 4.1 Build `engine_runtime` and `engine_editor` Debug
- [x] 4.2 Manual: idle rotate → semicircular dials facing camera
- [x] 4.3 Manual: orbit camera → semicircles update correctly
- [x] 4.4 Manual: drag rotation → full rings + ghost arc; release → semicircles return
- [x] 4.5 Manual: no flicker at grazing angles (bias tuning if needed)
