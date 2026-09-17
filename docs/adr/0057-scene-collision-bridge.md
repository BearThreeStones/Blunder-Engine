# Scene collision bridge: Unique colliders, static trimesh, queries, CCT

Kernel v0 stays an isolated Q32.32 World. This change is the **scene collision bridge**: Collider Unique, static triangle mesh, layer bitmask **and** string groups, ray/shapecast, Character Controller (`move_and_slide` / Unity CCT size — not Unreal CMC), C# API, Play, and editor collision wireframes. Physics units remain **SI metres** (identity to Q32.32), not Unreal centimetres.

Grill locked Character Controller **in this slice**. Missing trimesh data **skips** (no collision): no auto-box, no hard Import fail. Dynamic hollow trimesh is refused. Product scene is **DogWalk**, not Test/Sponza.

Domain: [CONTEXT.md — Physics](../../CONTEXT.md). Kernel: [ADR 0028](0028-physics-kernel-fixedpoint-lockstep.md).

**Considered options:** defer CCT to a later knife; Unreal CMC clone; Unreal cm; layers XOR groups; auto-box on missing collision; Test/Sponza as the acceptance scene; heightfields; treating GPU pick as the gameplay ray. Rejected: DogWalk walk, ice Area rays, and forest ~337 colliders need this split together, and centimetre scale would miss metre meshes.
