## 1. CMake — GLM intrinsics (INTERFACE target)

- [x] 1.1 In `engine/3rdparty/CMakeLists.txt`, add `target_compile_definitions(glm INTERFACE GLM_FORCE_INTRINSICS)` after `add_subdirectory(glm)`
- [x] 1.2 Confirm no other target redefines conflicting `GLM_FORCE_*` macros (grep `GLM_FORCE` in `engine/`)

## 2. CMake — MSVC AVX2 arch flag

- [x] 2.1 In `engine/src/runtime/CMakeLists.txt`, add `target_compile_options(engine_runtime PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>)`
- [x] 2.2 If additional engine targets link `glm` directly, apply the same MSVC option to those targets

## 3. Verification build (MSVC)

- [x] 3.1 Temporarily add `GLM_FORCE_MESSAGES` to the `glm` INTERFACE definitions (or a single TU) and rebuild `engine_runtime`
- [x] 3.2 Confirm build log contains `GLM: x86 64 bits with AVX2 instruction set build target`
- [x] 3.3 Remove `GLM_FORCE_MESSAGES` before finishing the change
- [x] 3.4 Full rebuild: `cmake --build build/vs2026-debug --config Debug --target engine_editor`

## 4. Smoke test

- [ ] 4.1 Launch `engine_editor`; orbit/pan viewport camera — no crash, correct rendering
- [ ] 4.2 Select entity; drag translate/rotate/scale gizmo — transforms update correctly
- [ ] 4.3 Optional: import a mesh asset to exercise `asset_manager_gltf` GLM paths

## 5. Documentation

- [x] 5.1 Add AVX2 CPU baseline note to `docs/agents/build.md` (MSVC `/arch:AVX2` on engine targets; Intel 2013+ / AMD 2015+ class CPUs)

## 6. Linux sanity (if available)

- [ ] 6.1 Confirm Linux GCC build still compiles with `GLM_FORCE_INTRINSICS` and without MSVC `/arch:AVX2`
