# Task 1.2 Report — Dedicated Mesh Preview offscreen RT + readback

## Status

Complete. `MeshPreviewOffscreenBackend` now owns a Vulkan offscreen target and
GPU-to-CPU staging buffer independently of both `RenderSystem::m_offscreen` and
`RenderSystem::m_camera_preview_offscreen`. It clears the dedicated target,
copies RGBA8 pixels into staging, waits for the one-shot submission, and returns
the CPU buffer through the existing `IMeshPreviewRenderBackend` service hook.
The Camera Preview header documents the separate owners. Multi-submesh/material
drawing remains intentionally deferred to task 1.3.

## Backend coverage (stub vs real)

| Backend | Role in tests | What it proves |
|---------|---------------|----------------|
| **`ClearReadbackMeshPreviewBackend`** (stub) | Injected into `MeshPreviewRenderService` for Intermediate-path service tests | Service wiring, load-source resolution, callback hooks, RGBA buffer sizing — **no GPU** |
| **`FailingMeshPreviewBackend`** (stub) | Injected for backend-failure path | GPU/backend error propagation and failure hooks — **no GPU** |
| **`MeshPreviewOffscreenBackend`** (real) | Direct unit tests + optional Vulkan harness | Owner tag, init rejection, lazy RT allocation, clear-color readback — **GPU when harness available** |

Service-layer readback assertions use the **stub** backend only. Real offscreen
RT + readback is exercised only via `MeshPreviewOffscreenBackend` tests.

## TDD evidence

- RED: `cmake --build build/vs2026-debug --config Debug --target
  mesh_preview_render_test` failed with C1083 because
  `mesh_preview_offscreen_backend.h` did not exist.
- GREEN: the same target built successfully, and
  `build/vs2026-debug/engine/src/tests/Debug/mesh_preview_render_test.exe`
  reported `mesh_preview_render_test: all passed`.
- Owner-enum tests assert Mesh Preview differs from Camera Preview and main
  viewport. Service Intermediate-path readback uses `ClearReadbackMeshPreviewBackend`
  (stub), not the real backend.

## Validation notes

`ctest -R "^mesh_preview_render_test$"` reported no discovered tests in the
existing generated build tree, so the built test executable was run directly.
The backend is Vulkan-only, matching the engine's implemented render path;
D3D12 remains a skeleton. The current GPU output is the clear frame until 1.3.

## Fix pass (review Important)

Addressed: real `MeshPreviewOffscreenBackend` was untested; report conflated
stub and real backend coverage.

**Command:**
```powershell
cmake --build build/vs2026-debug --config Debug --target mesh_preview_render_test -- /m:1 /p:CL_MPCount=1
.\build\vs2026-debug\engine\src\tests\Debug\mesh_preview_render_test.exe
```

**Output:**
```
mesh_preview_render_test: all passed (GPU readback verified)
Exit: 0
```

**New/updated tests:**
- `meshPreviewOffscreenBackendRejectsInvalidInit` — null/non-Vulkan init rejected;
  `offscreenTarget()` null before/after failed init; render without init fails (CPU)
- `meshPreviewOffscreenBackendGpuHarnessWhenAvailable` — WindowSystem +
  `RenderBackendFactory` Vulkan harness; `initialize` + `renderMeshPreview`;
  64×64 RGBA readback matches task 1.2 clear color; offscreen target allocated
- `renderUsesIntermediateWhenFinalMissing` labels stub backend explicitly
- Exit banner distinguishes `(GPU readback verified)` vs `(GPU readback not verified;
  CPU/stub coverage only)` when harness unavailable

**GPU gate:** harness runs when Vulkan loader + SDL window + backend create succeed;
otherwise prints skip reason and passes on CPU/stub coverage only — report does
not claim GPU readback unless banner says verified.

**Production tweak:** move default ctor to `.cpp` so `unique_ptr<VulkanBuffer>`
stays complete-type-safe for test TUs.
