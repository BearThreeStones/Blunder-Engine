## Context

Blunder has no rigid-body subsystem today (`SceneInstance` holds TRS POD entities; ECS World remains a dual-track destination). Upcoming frame-sync multiplayer needs lockstep-grade physics. Grilling + [ADR 0028](../../../docs/adr/0028-physics-kernel-fixedpoint-lockstep.md) locked a **self-built Physics Kernel v0**: isolated World, Q32.32 fixed math, golden suite on Win+Linux x64, game/scene APIs deferred. Domain language: [CONTEXT.md — Physics](../../../CONTEXT.md).

## Goals / Non-Goals

**Goals:**

- Headless **Physics World** + fixed `step(1/60)` with SI / Z-up gravity
- Reusable **Fixed math** (Q32.32) independent of float glm/`libm`
- Dynamic / Static / Kinematic (M1 pose-delta velocity), convex colliders, materials, stacking, per-body sleep
- Eight required goldens + module unit tests; **bit-identical** state across Win MSVC x64 and Linux Clang/GCC x64
- CMake targets so CI can run goldens without editor

**Non-Goals:**

- SceneInstance / ECS / Object bridge (post-v0 **Physics scene bridge**)
- CharacterController, C# physics API, Messages from physics
- CCD, joints, island sleep, triangle meshes, soft body, cloth, vehicles, 2D
- Multithreading; third-party solver as lasting truth
- Float hot-path simulation

## Decisions

1. **Self-built kernel (D1), not Jolt/PhysX as truth**  
   Lockstep + Q32.32 make an external float engine a calibration aid only (optional early V2), never the product oracle.  
   *Alternatives:* wrap Jolt; float custom solver + hope.

2. **Isolated Physics World (H1) for v0**  
   Goldens drive World directly; no EntityId/ObjectId in the kernel API.  
   *Alternatives:* hang state on SceneInstance; land ECS first.

3. **Fixed step API only (T1)**  
   `world.step(dt)` with default `dt = 1/60`; frame accumulators belong to a later host.  
   *Alternatives:* World-owned accumulator; variable dt.

4. **Q32.32 fixed scalar + Fixed math module (FP1/Q1/Scope1+2 reserve)**  
   All kernel integrates/solves in fixed; math is a standalone library for later lockstep gameplay. Scene bridge (later) converts to float for rendering.  
   *Alternatives:* Q22.10; IEEE strict float; physics-private helpers only.

5. **Body/Collider split; Dynamic+Static+Kinematic (B2/M1)**  
   Kinematic sets `target_pose`; step derives velocity from Δpose, participates in contacts, ends at target. Moving-platform-pushes-box is a required golden.  
   *Alternatives:* no Kinematic in v0; teleport-only kinematic.

6. **Per-collider materials (F1); per-body sleep (S1)**  
   Default restitution 0; island sleep deferred.  
   *Alternatives:* frictionless; island sleep in v0.

7. **Determinism gate = P1 bit-identical goldens**  
   Same scenario dumps (or in-memory state hashes of fixed fields) must match on Win MSVC x64 and Linux Clang/GCC x64. Single-threaded step with stable pair ordering.  
   *Alternatives:* same-machine only; ARM in v0 matrix.

8. **Contact pipeline sufficient for goldens, algorithm flexible**  
   Broadphase + convex narrowphase (GJK/EPA or equivalent) + iterative contact solve MUST pass the eight scenarios; exact solver (SI vs PGS iteration counts, Baumgarte constants) is implementation detail tuned under goldens — freeze ordering rules once goldens are green.  
   *Alternatives:* prescribe a textbook solver before any golden runs.

9. **Library layout**  
   Prefer `engine/src/runtime/core/math/fixed/` (or sibling) for Fixed math and `engine/src/runtime/function/physics/` for World/collision; static libs linked by a `physics_golden_suite` (or similar) test target. Exact folder names may follow existing CMake conventions when applying.  
   *Alternatives:* single monolithic physics TU; header-only only.

## Risks / Trade-offs

- **[Risk] Q32.32 range/precision surprises (large worlds, tiny epsilons)** → Keep v0 scenes near origin; document max safe extents; goldens use modest scales.  
- **[Risk] Mul/div widening differs across MSVC/GCC intrinsics** → Centralize 128-bit mul/div in Fixed math with shared tests; forbid ad-hoc assembly outside that module.  
- **[Risk] Stacking/sleep flaky under weak solver** → Tune under goldens 2–3/8; do not ship v0 without sleep/stack pass.  
- **[Risk] Scope creep into scene bridge / character** → Explicit non-goals; bridge is next change.  
- **[Trade-off] Slower path to visible editor physics** → Accepted: correctness/lockstep before presentation.  
- **[Trade-off] No third-party solver expertise borrowed** → Mitigate with optional early calibration dumps, then discard as truth.

## Migration Plan

- Greenfield module; no existing physics data to migrate.  
- Rollback = remove/disable physics CMake targets; no scene format changes in v0.  
- After archive: follow-on change for Physics scene bridge.

## Open Questions

- Exact Fixed math public type names (`Fixed32`, `Fix64`, …) — choose at apply time; keep Q32.32 semantics.  
- Golden comparison: full state blob vs selective field asserts — prefer selective asserts + optional full dump for CI bisect.  
- Whether Linux CI uses Clang or GCC as the bit-identical peer — either is fine; pick one primary in tasks and stick to it for v0.
