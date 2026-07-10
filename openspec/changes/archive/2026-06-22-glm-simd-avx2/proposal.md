## Why

Blunder Engine uses GLM 1.1.0 for CPU-side math (gizmo, camera, scene transforms, forward shading uniforms) but currently compiles with **no SIMD intrinsics path** enabled: `GLM_CONFIG_SIMD` is off because neither `GLM_FORCE_INTRINSICS` nor compiler ISA flags are set. Gizmo drag, camera orbit, and matrix-heavy editor paths leave performance on the table. **Option B** from exploration — `GLM_FORCE_INTRINSICS` plus MSVC `/arch:AVX2` — enables GLM's AVX2 matrix/vector implementations on modern desktop CPUs with a small, centralized CMake change.

## What Changes

- Define `GLM_FORCE_INTRINSICS` on the `glm::glm` INTERFACE target so all consumers share one GLM configuration.
- Add MSVC `/arch:AVX2` for engine targets that link GLM (at minimum `engine_runtime`), allowing GLM to detect `GLM_ARCH_AVX2` and use AVX2 intrinsics in `func_matrix.inl`, `type_mat4x4.inl`, etc.
- Document the **AVX2 CPU baseline** (Intel 2013+ / AMD 2015+) in build docs.
- Add a one-time verification step: `GLM_FORCE_MESSAGES` build log check and editor smoke test (gizmo + camera).
- No API or shader changes; no user-visible feature flags.

## Capabilities

### New Capabilities

- `engine-glm-simd`: Build-time GLM SIMD configuration — intrinsics enablement, MSVC AVX2 arch flag, cross-target consistency, and verification criteria.

### Modified Capabilities

- _(none — implementation-only build configuration; no existing OpenSpec spec requirements change)_

## Impact

- **Primary**: `engine/3rdparty/CMakeLists.txt` (`glm` INTERFACE compile definitions), `cmake/compiler.cmake` or `engine/src/runtime/CMakeLists.txt` (MSVC `/arch:AVX2`).
- **Secondary**: `engine/src/runtime/pch.h` (rebuild only if defines moved there instead of CMake — design prefers CMake target).
- **Aligned gentypes**: SIMD enablement auto-activates GLM aligned vector types; struct padding may increase slightly (e.g. `vec3` → 16 bytes). No `sizeof(glm::...)` usage found; all TUs must share the same defines via `glm` target.
- **Platforms**: Windows/MSVC is the primary target; Linux GCC build should retain `GLM_FORCE_INTRINSICS` (SSE/AVX per compiler flags) — AVX2 flag is MSVC-scoped.
- **Not in scope**: Viewport FPS (Skia/readback bottleneck), GPU shader math, vendored KTX copy of GLM under `engine/3rdparty/ktx/other_include/glm/`.
- **Risk**: Binaries won't run on pre-AVX2 x64 CPUs; acceptable for a modern game-engine editor.
