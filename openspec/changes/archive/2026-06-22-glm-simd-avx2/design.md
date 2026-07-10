## Context

Blunder links GLM 1.1.0 as a header-only INTERFACE target (`engine/3rdparty/CMakeLists.txt`). `engine_runtime` includes GLM via PCH (`pch.h`) and links `glm`. No `GLM_FORCE_*` macros or `/arch` flags are set today.

GLM SIMD activation chain (`glm/detail/setup.hpp`, `glm/simd/platform.h`):

1. `GLM_FORCE_INTRINSICS` → arch auto-detect from compiler predefined macros (`__AVX2__`, `__SSE2__`, etc.)
2. `(GLM_LANG & GLM_LANG_CXXMS_FLAG) && (GLM_ARCH & GLM_ARCH_SIMD_BIT)` → `GLM_CONFIG_SIMD = ENABLE`
3. SIMD enable → `GLM_CONFIG_ALIGNED_GENTYPES` auto-enabled

Without step 1, x64 MSVC defaults to `GLM_ARCH_X86` (no `SIMD_BIT`) and all SIMD code paths in `func_matrix.inl`, `type_mat4x4.inl`, etc. are compiled out.

MSVC does not define `__AVX2__` unless `/arch:AVX2` is passed — hence Option B requires **both** the macro and the compiler flag.

## Goals / Non-Goals

**Goals:**

- Enable GLM AVX2 intrinsics for all targets linking `glm::glm`.
- Centralize configuration in CMake (single source of truth).
- Document CPU baseline and verification steps.
- Keep Linux GCC build working (intrinsics without MSVC-specific `/arch`).

**Non-Goals:**

- Viewport FPS optimization (Skia/readback path unchanged).
- Custom hand-written SIMD in engine code.
- Upgrading GLM version.
- Changing public engine APIs or math types.
- Enabling GLM swizzle, `GLM_FORCE_PURE`, or `GLM_FORCE_DEFAULT_ALIGNED_GENTYPES` beyond what SIMD auto-enables.

## Decisions

### 1. Put `GLM_FORCE_INTRINSICS` on the `glm` INTERFACE target

**Choice:** `target_compile_definitions(glm INTERFACE GLM_FORCE_INTRINSICS)` in `engine/3rdparty/CMakeLists.txt`.

**Rationale:** Propagates to every consumer via `target_link_libraries(... glm)`. Avoids duplicating defines in `pch.h` or per-target CMake. Matches GLM's recommended pattern for `add_subdirectory` integration.

**Alternatives considered:**

| Alternative | Why rejected |
|-------------|--------------|
| Define in `pch.h` only | TUs that include GLM without PCH or without linking `glm` get inconsistent config |
| `GLM_FORCE_AVX2` macro alone | Works but bypasses auto-detect; couples GLM arch to a force macro instead of compiler ISA |
| Global `add_compile_definitions` at root | Affects non-GLM third-party code unnecessarily |

### 2. MSVC `/arch:AVX2` on engine targets, not globally

**Choice:** Add `$<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>` to `engine_runtime` (and any future engine target linking GLM). Optionally hoist to a shared `blunder_engine_compile_options` interface if one exists.

**Rationale:** Limits ISA requirement to engine binaries. Third-party submodules (SDL, assimp, etc.) keep their default arch. GLM is header-only — the flag must be on the **consumer** TU that instantiates GLM templates.

**Alternatives considered:**

| Alternative | Why rejected |
|-------------|--------------|
| `/arch:AVX2` in root `compiler.cmake` for all targets | Over-broad; increases build time and ISA lock-in for deps |
| SSE2 only (Option A) | Lower gain; user explicitly chose Option B |
| `/arch:AVX` (not AVX2) | AVX2 gives better matrix paths in GLM 1.1; AVX alone is an odd middle ground |

### 3. Do not add defines to `pch.h`

**Choice:** CMake-only configuration.

**Rationale:** PCH change forces full `engine_runtime` rebuild anyway when applied; keeping defines on the `glm` target is cleaner and survives PCH edits.

### 4. Linux: intrinsics only, no forced AVX2 flag in this change

**Choice:** `GLM_FORCE_INTRINSICS` via INTERFACE; GCC gets SSE2+ on x86_64 by default. Optional `-mavx2` for Linux can be a follow-up.

**Rationale:** Primary dev platform is Windows/MSVC per `docs/agents/cursor-cloud.md`. Cloud Linux agent build should still compile; AVX2 on GCC is a separate decision.

### 5. Verification via build log + smoke test, not perf regression gate

**Choice:** One-time `GLM_FORCE_MESSAGES` check during implementation; standard editor smoke test. No CI perf benchmark required.

**Rationale:** GLM vendored perf tests exist (`engine/3rdparty/glm/test/perf/`) but are not part of project CI. Marginal CPU win is hard to gate; correctness (no crash, gizmo/camera work) is the bar.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Pre-AVX2 CPUs cannot run editor | Document baseline in `docs/agents/build.md`; acceptable for modern desktop editor |
| Aligned gentypes change struct padding | All TUs share `glm` INTERFACE defines; no `sizeof(glm::...)` found; smoke test gizmo structs |
| ODR violation if one TU lacks defines | Only link GLM through `glm` target; do not `#include` GLM with different defines |
| PCH + define change → long rebuild | Expected one-time cost |
| KTX vendored GLM unaffected | Isolated copy; no action unless KTX tests fail |
| `/arch:AVX2` on MSVC increases compile time slightly | Scoped to engine targets only |

## Migration Plan

1. Add `GLM_FORCE_INTRINSICS` to `glm` INTERFACE target.
2. Add MSVC `/arch:AVX2` to `engine_runtime`.
3. Full rebuild `engine_editor`.
4. Temporarily enable `GLM_FORCE_MESSAGES`, confirm log shows AVX2 build target.
5. Smoke test: launch editor, orbit camera, drag transform gizmo, import mesh.
6. Remove `GLM_FORCE_MESSAGES` before merge.
7. Update `docs/agents/build.md` with AVX2 CPU note.

**Rollback:** Revert CMake changes; no data migration.

## Open Questions

- _(none — Option B scope is settled from exploration)_
