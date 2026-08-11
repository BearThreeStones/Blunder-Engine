## Why

Blunder has no rigid-body simulation, and upcoming **frame-sync / lockstep** multiplayer needs a physics core whose results are **bit-identical across platforms** — IEEE float solvers (and third-party engines built on them) are a poor lasting truth source. We need a closable **Physics Kernel v0** before scene bridges or Character APIs. Decision: [ADR 0028](../../../docs/adr/0028-physics-kernel-fixedpoint-lockstep.md); domain: [CONTEXT.md — Physics](../../../CONTEXT.md).

## What Changes

- Add a native **Physics Kernel** and isolated **Physics World** (authoritative body/collider state; not SceneInstance / ECS / Object for v0).
- Fixed **Physics step** `dt = 1/60`; SI units with default gravity `(0, 0, -9.81)` in Z-up space.
- Simulation scalar **Q32.32** via a reusable **Fixed math** module (independent of float `glm` / platform `libm` on the hot path).
- **RigidBody** + **Collider** (box / sphere / capsule); motion types **Dynamic / Static / Kinematic** (Kinematic uses target pose → derived velocity, can push Dynamics).
- Per-Collider **Physics material** (friction + restitution; default restitution 0); resting contact, stacking, **per-body sleep**.
- **Physics golden suite** (eight required scenarios) + module unit tests; **bit-identical** World state on **Windows MSVC x64** and **Linux Clang or GCC x64**.
- CMake/test harness wiring so goldens run headless (no editor/Play required for v0 Done).

**Out of scope (v0):** CCD; joints beyond contact; island sleep; triangle meshes / heightfields; soft body / cloth / vehicles / 2D; CharacterController or C# gameplay physics API; SceneInstance / ECS / Object projection (**Physics scene bridge** is the first *post*-v0 milestone); multithreading; adopting Jolt/PhysX/Bullet as the lasting solver.

## Capabilities

### New Capabilities
- `fixed-math`: Q32.32 scalar/vector/quaternion math and physics-needed transcendentals/sqrt; reusable module; no float glm/`libm` on simulation path
- `physics-world`: Physics World container, RigidBody/Collider, motion types, materials, fixed step, forces/gravity, sleep
- `physics-collision`: Broad/narrow phase for convex pairs, contact generation and resolution sufficient for stacking / resting / kinematic push
- `physics-golden-suite`: Eight required numeric scenarios; Win+Linux x64 bit-identical gate; module unit tests for isolated algorithms

### Modified Capabilities
- _(none — no existing physics specs; scene/script surfaces unchanged in v0)_

## Impact

- Engine: new physics + fixed-math libraries/targets under `engine/`; headless golden test executable(s); CMake presets for Win and Linux CI
- Docs: CONTEXT Physics terms; ADR 0028
- Non-impact (v0): editor viewport, Play Mode, C-ABI/Blunder.Api gameplay physics, DogWalk Character content
- Follow-ons (not this change): Physics scene bridge → later island sleep / CCD / joints / meshes / Character API / ordered multithreading
