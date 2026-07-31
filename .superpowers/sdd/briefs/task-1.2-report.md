# Task 1.2 Report — Dedicated Mesh Preview offscreen RT + readback

## Status

Complete. `MeshPreviewOffscreenBackend` now owns a Vulkan offscreen target and
GPU-to-CPU staging buffer independently of both `RenderSystem::m_offscreen` and
`RenderSystem::m_camera_preview_offscreen`. It clears the dedicated target,
copies RGBA8 pixels into staging, waits for the one-shot submission, and returns
the CPU buffer through the existing `IMeshPreviewRenderBackend` service hook.
The Camera Preview header documents the separate owners. Multi-submesh/material
drawing remains intentionally deferred to task 1.3.

## TDD evidence

- RED: `cmake --build build/vs2026-debug --config Debug --target
  mesh_preview_render_test` failed with C1083 because
  `mesh_preview_offscreen_backend.h` did not exist.
- GREEN: the same target built successfully, and
  `build/vs2026-debug/engine/src/tests/Debug/mesh_preview_render_test.exe`
  reported `mesh_preview_render_test: all passed`.
- The tests assert the Mesh Preview owner differs from Camera Preview and the
  main viewport, and that the service returns a correctly sized RGBA readback
  supplied by its backend.

## Validation notes

`ctest -R "^mesh_preview_render_test$"` reported no discovered tests in the
existing generated build tree, so the built test executable was run directly.
The backend is Vulkan-only, matching the engine's implemented render path;
D3D12 remains a skeleton. The current GPU output is the clear frame until 1.3.
