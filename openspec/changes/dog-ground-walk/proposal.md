# Proposal: Dog ground walk (COL bake → sphere CCT)

## Why

DogWalk `se-world.scene.asset` has zero colliders: flatten/import omit `COL-*`, and Godot extras lack `triangles`. Chocomel cannot stand on real ground. Locked grill: Q1–Q17 in Project Context `dog-ground-walk-proposal.md`.

## What Changes

- **A — COL bake:** Flatten + runtime import extract `COL-*` mesh verts → Static `triangleMesh` Unique (no MeshRenderer). Skip `*detection*` Areas. Rebake se-world so `"collider"` count ≠ 0.
- **B — Sphere CCT + sphere×trimesh sweep:** Unique sphere mode; reliable continuous sphere vs static trimesh; host tests.
- **C–E:** C# Chocomel locomotion; Editor collision debug (default off); fixture QC + wire into se-world.

## Out of scope

Pinda/leash, snow/ice speed, jump, full AnimationTree, fake floor boxes, GEO-as-collision, heightfield, full Godot layer-name map.

## Capabilities

### New Capabilities

- `col-bake`: COL-* → static trimesh Unique on flatten/import; se-world rebake
- `sphere-cct`: CharacterController sphere mode + sphere×trimesh sweep
- `chocomel-locomotion`: C# input+gravity+MoveAndSlide (DogWalk)
- `collision-debug-dog`: Editor overlay dog sphere + static colliders, default off

## Impact

- Engine: `se_world_flatten`, `gltf_scene_importer`, `gltf_collision_extras`, physics character/world, CCT Unique
- Content: DogWalk `se-world.scene.asset` rebake (not in engine git)
