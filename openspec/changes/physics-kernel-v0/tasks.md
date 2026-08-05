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

- [ ] 3.1 Deterministic broadphase (stable pair order) for World bodies
- [ ] 3.2 Convex narrowphase (GJK/EPA or equivalent) for box/sphere/capsule pairs; module unit tests
- [ ] 3.3 Contact constraint solve (iterative) for resting contact; restitution 0 path
- [ ] 3.4 Friction sufficient for incline golden
- [ ] 3.5 Integrate contacts with Dynamic/Static/Kinematic (M1 push)
- [ ] 3.6 Per-body sleep + wake on hit/force

## 4. Golden suite harness

- [ ] 4.1 Headless golden test executable/target (no editor/Play)
- [ ] 4.2 Scenario 1: free fall under gravity
- [ ] 4.3 Scenario 2: Dynamic rests on Static
- [ ] 4.4 Scenario 3: stack ≥3 boxes
- [ ] 4.5 Scenario 4: sphere–box and capsule–box resting
- [ ] 4.6 Scenario 5: frictional incline
- [ ] 4.7 Scenario 6: Kinematic platform lifts/pushes Dynamic box
- [ ] 4.8 Scenario 7: impact energy bound (restitution 0)
- [ ] 4.9 Scenario 8: sleep then wake on hit
- [ ] 4.10 Cross-platform gate: same goldens bit-identical on Win MSVC x64 and Linux Clang/GCC x64 (CI or documented dual run)

## 5. Docs / closeout

- [ ] 5.1 Confirm CONTEXT Physics + ADR 0027 match implementation names
- [ ] 5.2 Note follow-on: Physics scene bridge (not in this change)
- [ ] 5.3 Manual/CI checklist: how to run Fixed unit tests + golden suite on Win and Linux
