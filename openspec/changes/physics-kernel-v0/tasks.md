## 1. Fixed math module

- [x] 1.1 Add Fixed math library/target (Q32.32 scalar) under engine math; no float glm/`libm` on core ops
- [x] 1.2 Implement add/sub/mul/div with portable 128-bit widening (MSVC `_mul128`/`_div128` or equivalent; GCC/Clang `__int128`)
- [x] 1.3 TDD: scalar ops + bit-identical asserts on Win MSVC x64
- [x] 1.4 Fixed Vec3 + orientation (quat or equivalent) + normalize
- [x] 1.5 Sqrt / InvSqrt (or subset required by later collision) via Fixed math; unit tests for bit patterns
- [x] 1.6 Confirm Linux Clang or GCC x64 build of Fixed math unit tests (pick one primary peer and document in CMake)

## 2. Physics World skeleton

- [x] 2.1 Add physics library/target + Physics World create/destroy
- [x] 2.2 RigidBody create/destroy; pose, velocity, mass; motion types Dynamic/Static/Kinematic
- [x] 2.3 Collider attach (box, sphere, capsule) + per-collider material (friction, restitution default 0)
- [x] 2.4 `world.step(dt)` with default `dt = 1/60`; gravity default `(0,0,-9.81)` overrideable
- [x] 2.5 Force/impulse application API sufficient for goldens (at least clear forces + gravity integration for Dynamic)
- [x] 2.6 Kinematic target pose → Δpose velocity path wired into step (even before full contact)

## 3. Collision and solve

- [x] 3.1 Deterministic broadphase (stable pair order) for World bodies
- [x] 3.2 Convex narrowphase (GJK/EPA or equivalent) for box/sphere/capsule pairs; module unit tests
- [x] 3.3 Contact constraint solve (iterative) for resting contact; restitution 0 path
- [x] 3.4 Friction sufficient for incline golden
- [x] 3.5 Integrate contacts with Dynamic/Static/Kinematic (M1 push)
- [x] 3.6 Per-body sleep + wake on hit/force

## 4. Golden suite harness

- [x] 4.1 Headless golden test executable/target (no editor/Play)
- [x] 4.2 Scenario 1: free fall under gravity
- [x] 4.3 Scenario 2: Dynamic rests on Static
- [x] 4.4 Scenario 3: stack ≥3 boxes
- [x] 4.5 Scenario 4: sphere–box and capsule–box resting
- [x] 4.6 Scenario 5: frictional incline
- [x] 4.7 Scenario 6: Kinematic platform lifts/pushes Dynamic box
- [x] 4.8 Scenario 7: impact energy bound (restitution 0)
- [x] 4.9 Scenario 8: sleep then wake on hit
- [x] 4.10 Cross-platform gate: same goldens bit-identical on Win MSVC x64 and Linux Clang/GCC x64 (CI or documented dual run)

## 5. Docs / closeout

- [x] 5.1 Confirm CONTEXT Physics + ADR 0027 match implementation names
- [x] 5.2 Note follow-on: Physics scene bridge (not in this change)
- [x] 5.3 Manual/CI checklist: how to run Fixed unit tests + golden suite on Win and Linux

### Implementation names (5.1)

| Domain (CONTEXT / ADR) | Code / CMake |
|------------------------|--------------|
| Physics fixed scalar (Q32.32) | `Blunder::Fixed`, `FixedVec3`, `FixedQuat` |
| Physics World | `Blunder::PhysicsWorld` (`engine/src/runtime/function/physics/`) |
| Fixed math library | `blunder_fixed_math` (`engine/src/runtime/core/math/fixed/`) |
| Physics kernel library | `blunder_physics` |
| Physics golden suite | `physics_golden_suite` |

CONTEXT Physics section and [ADR 0027](../../../docs/adr/0027-physics-kernel-fixedpoint-lockstep.md) use domain terms; no vocabulary drift — no CONTEXT edit required.

### Follow-on (5.2)

**Physics scene bridge** (post–v0, not in this change): project `PhysicsWorld` body poses into `SceneInstance` / future ECS Transform so simulation is visible in the host. See CONTEXT — Physics scene bridge. Do not bundle into kernel v0 closure.

### CI / manual run (5.3)

**Windows (MSVC x64 Debug)**

```powershell
cmake --build build/vs2026-debug --config Debug --target fixed_math_test physics_golden_suite
ctest --test-dir build/vs2026-debug/engine/src/tests -C Debug -R "fixed_math|physics" --output-on-failure
```

**Linux (Clang or GCC x64)**

```bash
cmake --build build/linux-debug --target fixed_math_test physics_golden_suite
ctest --test-dir build/linux-debug/engine/src/tests -R "fixed_math|physics" --output-on-failure
./build/linux-debug/engine/src/tests/physics_golden_suite \
  --compare engine/src/tests/fixtures/physics_golden/win-msvc-x64.dump
```

Cross-platform bit-identical gate: `physics_golden_suite_cross_platform` CTest (runs `--compare` when reference dump exists). Reference: `engine/src/tests/fixtures/physics_golden/win-msvc-x64.dump`.
