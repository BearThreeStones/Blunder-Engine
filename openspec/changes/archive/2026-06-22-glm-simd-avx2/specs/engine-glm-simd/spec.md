## ADDED Requirements

### Requirement: GLM intrinsics enabled project-wide for glm consumers

The build system SHALL define `GLM_FORCE_INTRINSICS` on the `glm::glm` INTERFACE target so every translation unit that links `glm` compiles GLM with intrinsics-based SIMD support enabled.

#### Scenario: engine_runtime compiles with intrinsics define

- **WHEN** `engine_runtime` is configured and built
- **THEN** the compile command line for its translation units includes `GLM_FORCE_INTRINSICS` (via INTERFACE propagation from the `glm` target)

#### Scenario: consistent GLM config across linked targets

- **WHEN** any engine target links `glm::glm` and includes `<glm/glm.hpp>`
- **THEN** `GLM_CONFIG_SIMD` is enabled in that translation unit (verifiable with a temporary `GLM_FORCE_MESSAGES` build)

### Requirement: MSVC engine builds target AVX2 ISA

On MSVC, engine targets that link GLM SHALL be compiled with `/arch:AVX2` so GLM detects `GLM_ARCH_AVX2` and uses AVX2 matrix/vector intrinsics.

#### Scenario: MSVC compile flags include AVX2

- **WHEN** `engine_runtime` is built with MSVC (e.g. VS 2026 Debug configuration)
- **THEN** compile options for `engine_runtime` translation units include `/arch:AVX2`

#### Scenario: GLM reports AVX2 build target on MSVC

- **WHEN** a translation unit includes `<glm/glm.hpp>` with `GLM_FORCE_MESSAGES` defined before the include during a one-time verification build
- **THEN** the build log contains a GLM message indicating x86 64-bit AVX2 instruction set (e.g. `GLM: x86 64 bits with AVX2 instruction set build target`)

### Requirement: Linux GCC build remains compilable

The Linux development build (GCC, per `docs/agents/cursor-cloud.md`) SHALL continue to compile successfully with `GLM_FORCE_INTRINSICS` enabled, without requiring MSVC-specific `/arch:AVX2`.

#### Scenario: Linux configure and build succeeds

- **WHEN** the project is configured and built on Linux with GCC per project documentation
- **THEN** the build completes without GLM-related compile errors

### Requirement: Editor smoke test passes after SIMD enablement

After enabling GLM SIMD (Option B), the editor SHALL remain functional for core math-heavy interactions.

#### Scenario: Camera and gizmo interaction

- **WHEN** the user launches `engine_editor`, orbits the viewport camera, and drags the transform gizmo on a selected entity
- **THEN** the viewport renders correctly with no crashes or visibly broken transforms

### Requirement: CPU baseline documented

Project build documentation SHALL state that Windows editor binaries require an x64 CPU with AVX2 support when this change is applied.

#### Scenario: Build docs mention AVX2

- **WHEN** a developer reads `docs/agents/build.md`
- **THEN** the document includes a note that MSVC engine builds use `/arch:AVX2` and require AVX2-capable hardware
