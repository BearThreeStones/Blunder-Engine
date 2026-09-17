# Design

## Context

See proposal.md for why. Grill locked Character Controller in this slice, static-only trimesh, layers **and** groups, SI metres (Q32.32), Play + Viewport wireframe + edit-mode rays, and skip-on-missing-trimesh.

Today: `PhysicsWorld` is box/sphere/capsule + Dynamic/Static/Kinematic, no trimesh, no queries, no layer bits. `PhysicsManager` is commented out of `RuntimeGlobalContext`. Unique attachments are Camera, Light, Skeleton, AnimationTree. C# `ObjectHandle.Position` is a teleport. Test Chocomel locomotion writes Position. Product scene is DogWalk, not Test/Sponza.

Kernel v0 and [ADR 0028](../../../docs/adr/0028-physics-kernel-fixedpoint-lockstep.md) stay the solver. This change is the post-v0 host + query + Unique + CCT surface. Decision record: [ADR 0057](../../../docs/adr/0057-scene-collision-bridge.md).

## Goals / Non-Goals

**Goals:**

- One Physics World per `SceneInstance` (editor Live document, Play Process world).
- Unique → kernel bodies in SI metres (float TRS → Q32.32 identity scale).
- Static trimesh attach + primitive queries that can hit it.
- CCT sweep/slide as the walk API.
- Editor overlay wires; edit queries share Play’s query entry.

**Non-Goals (design-level):**

- Replacing Q32.32 or adopting PhysX/Chaos/Jolt as truth.
- Editor-time Dynamic sandbox (Edit queries static/kinematic/area poses; Play steps).
- Convex Unique, heightfield, CCD.
- Creating Objects for every static collider (337 forest meshes stay entities).
- Hierarchy Create… Collider (Add… only, like Skeleton).

## Decisions

1. **Two Unique kinds: Collider and Character Controller**  
   Collider Unique holds shape (Box / Sphere / Capsule / Triangle Mesh), body kind (Static / Kinematic / Area), layer, mask, metre sizes. Character Controller Unique holds capsule radius/height (local +Z), slope limit, step height, floor snap, skin, mask. CCT does **not** use a co-located Collider Unique for the sweep capsule (avoids double capsules). Both Unique on one entity is allowed; CCT sweep ignores a trimesh Collider on that entity.  
   *Alternatives:* one Unique with a “character” mode (hides Area/trimesh); Unity-style CCT-is-a-Collider (fights Unique-at-most-once + Area).

2. **Triangle Mesh is Static-only in the kernel**  
   `attachTriangleMesh` (name at apply) on Static bodies. Kinematic/Area/Dynamic + trimesh at the Unique: no kernel shape (skip, log, no auto-box). Movers: box/sphere/capsule only.  
   *Alternatives:* ComplexAsSimple for Kinematic platforms (not needed for DogWalk walk); convex hull Unique (follow-on).

3. **Missing trimesh is skip**  
   No extras, empty triangle list, or failed cook → that mesh has no collider. Import continues. No AABB box. No import error that aborts the rest.  
   *Alternatives:* auto-box (Grill rejected); hard fail (blocks forest import).

4. **Layers and groups are two fields**  
   32-bit layer (what I am) + mask (what I see). Queries and CCT take a mask. Groups are string names on the **entity** (`groups` in the scene document), not a physics filter. Hit payload includes groups. `TerrainIce` / `TerrainSnow` / `LeashPivots` are group names. Inspector: layer bits 1–32 and a group list.  
   *Alternatives:* Unreal profiles instead of bits (sugar later); encoding ice as a layer (breaks Godot scripts keyed on group).

5. **Area is query-only**  
   Area colliders generate no solid contacts. Rays/shapecasts hit them only when the query sets collide-with-areas (default **false**, Godot). Terrain detector passes true + layer 5.  
   *Alternatives:* Areas always hit (too noisy); Areas as a layer bit only (still need collide-with-areas).

6. **Queries are primitives; they may hit trimesh**  
   Ray, BoxCast, SphereCast, CapsuleCast. No trimesh shapecast shape. Results in metres. Kernel stays Q32.32; C# sees float. Same function for Edit and Play (different World: Live vs Player).  
   *Alternatives:* editor-only GPU pick as “ray” (wrong for ice/leash); float-only queries (breaks lockstep path).

7. **CCT size is Godot `move_and_slide` / Unity CCT, not Unreal CMC**  
   C#: `Velocity`, `MoveAndSlide()`, `IsOnFloor` / `IsOnWall` / `IsOnCeiling`. Gravity is script-applied. `Object.Position` write is teleport (no sweep) — spawn/reset only. Capsule axis = entity local +Z. Inspector Height is full height including hemispheres; kernel `half_height = max(0, height/2 − radius)`.  
   *Alternatives:* Unreal CMC walking/falling modes (too large); drive walk with Dynamic rigidbody.

8. **Host: enable physics in `RuntimeGlobalContext`; one World per SceneInstance**  
   Instantiate / Unique / TRS commit rebuilds kernel colliders from Unique. Play accumulates `dt = 1/60` and `step`s. Pause skips steps and Tick. Edit does not step Dynamics. CCT sweeps at `MoveAndSlide` time against the current World.  
   *Alternatives:* one process-global World (Play vs Edit clash); step in Edit (not grilled).

9. **Add… Collider / Character Controller do not materialize Object**  
   Same as Camera/Light. Groups still serialize on the entity. C# `FindObjectsInGroup` returns bound Objects only. Ray hits still expose groups when Object is null. Characters that Tick already have Behaviours → Objects.  
   *Alternatives:* always Object (337 extra ClassDB objects).

10. **Collision wireframe is an Editor Overlay**  
    Line topology for box/sphere/capsule/CCT and trimesh edges, all Collider/CCT Uniques in the open scene (Light Gizmo pattern). Player never draws it. Reuse overlay line pass; do not present 3D to HWND.  
    *Alternatives:* selected-only (harder to see forest); Player debug draw (authorship chrome leak).

11. **C-ABI v13 + Blunder.Api façades**  
    NativeAbi gains query, group, and CCT function pointers. Completeness requires them. Managed `Physics`, entity groups on `ObjectHandle` when bound, `CharacterController` façade like `AnimationTree`.  
    *Alternatives:* Physics as Behaviour (not Unique); P/Invoke a second image (forbidden).

12. **DogWalk is the human scene; engine tests stay synthetic**  
    Fixtures and Play Behaviour live under `E:\Blunder Projects\DogWalk`. Engine `engine/src/tests/` cover kernel/query/serialize. Do not retarget Test `chocomel_locomotion` or Sponza as this change’s main scene. Forest ~5k expand is a later change; this slice must still accept a DogWalk scene that *has* colliders.  
    *Alternatives:* pile colliders into Test (Grill rejected).

13. **glTF collision extras**  
    When Import/scene-from-glTF sees blender-studio / Godot `collision_info` (or the same extras key that exporter actually writes — confirm at apply against `gltf_import_script.gd`), attach Static Triangle Mesh Unique on that mesh entity. Absent/empty → skip.  
    *Alternatives:* derive trimesh from every render mesh (too heavy, wrong for visuals-only).

## Risks / Trade-offs

- **[Risk] Trimesh vs capsule tunneling at high speed** → CCT uses sweep (not teleport); this slice still has no CCD. Cap debug speed in fixtures; CCD is follow-on.
- **[Risk] Q32.32 range on large forest extents** → Keep queries in metres; goldens near origin; document max safe extent from kernel v0.
- **[Risk] 337 trimeshes + wireframe cost** → Overlay is editor-only; kernel BVH/grid is an implementation detail under query tests; no Player wireframe.
- **[Risk] Edit World stale after gizmo move** → Rebuild collider pose on TRS Command seal (and live gizmo sample if queries run during drag — prefer seal + Inspector; live sample if edit rays must track the drag).
- **[Trade-off] No Object on static colliders** → `FindObjectsInGroup` misses entity-only groups; hits still carry group strings (enough for ice ray).
- **[Trade-off] CCT not in the contact solver** → Moving platforms that must push the dog need Kinematic Unique + later CCT platform velocity (Godot floor velocity). Out of this slice unless a fixture needs it; static forest does not.

## Migration Plan

1. Kernel trimesh + queries with headless tests (no editor).
2. Unique serialize + Inspector Add… + Commands.
3. Host wiring in editor and `engine_player`.
4. CCT + C# API.
5. Overlay + edit queries.
6. DogWalk fixture scene; Import skip policy.
7. Rollback = revert the change; scenes with unknown Unique keys must load by ignoring unknown keys **or** this change ships loaders together (do not leave DogWalk unopenable). Prefer: unknown Unique keys warn and skip, like missing trimesh.

No centimetre bake to migrate. Existing Test Position locomotion stays until a later DogWalk walk Behaviour.

## Open Questions

- Exact glTF extras key string if it is not `collision_info` — read Godot `gltf_import_script.gd` at apply; behavior (skip vs attach) does not change.
