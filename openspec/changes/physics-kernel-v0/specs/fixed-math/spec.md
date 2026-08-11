## ADDED Requirements

### Requirement: Q32.32 scalar type
The engine SHALL provide a Physics fixed scalar type representing binary fixed-point **Q32.32** in a signed 64-bit integer (32 integer bits, 32 fractional bits). Arithmetic used by the Physics Kernel SHALL operate on this type (and composites built from it), not on IEEE `float`/`double`, on the simulation hot path.

#### Scenario: Construct and add
- **WHEN** two Q32.32 values representing `1` and `2` are added
- **THEN** the result equals the Q32.32 representation of `3` on both Windows MSVC x64 and Linux Clang or GCC x64

### Requirement: Fixed composites for physics
Fixed math SHALL provide vector (at least 3) and quaternion (or equivalent orientation) types built from the Q32.32 scalar for use by the Physics Kernel.

#### Scenario: Normalize direction
- **WHEN** a fixed Vec3 axis-aligned unit vector is normalized via Fixed math
- **THEN** the result is a unit-length fixed vector without calling platform `libm` or float `glm` on that path

### Requirement: Physics transcendentals without libm
Fixed math SHALL provide the transcendental and root operations the Physics Kernel needs for v0 (at least sqrt and/or reciprocal sqrt as required by the implemented contact/integration path). Those operations SHALL NOT call platform `libm` (`sqrtf`, `std::sin`, etc.) on the simulation path.

#### Scenario: Sqrt determinism
- **WHEN** Fixed sqrt (or InvSqrt-based path) is evaluated for the same positive Q32.32 input on Win MSVC x64 and Linux Clang/GCC x64
- **THEN** the bit pattern of the result is identical

### Requirement: Module independence and reuse reserve
Fixed math SHALL be packaged as a reusable engine module that does not depend on float `glm` for its core operations. Kernel v0 SHALL consume it for physics only; the module SHALL remain usable by a later lockstep gameplay layer without rewriting the scalar type.

#### Scenario: Physics links Fixed math without glm
- **WHEN** the Physics Kernel target is linked
- **THEN** its simulation math path does not require float glm types as the source of truth for body integration
