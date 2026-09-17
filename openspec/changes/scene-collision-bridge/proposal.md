# Proposal

## Why

Physics Kernel v0 can step isolated box/sphere/capsule bodies, but DogWalk cannot stand on the forest: there is no Collider Unique, no static trimesh for the ~337 Godot colliders, no layer/group query surface, no Character Controller, and Play locomotion still writes `Object.Position`. October “forest can stand” needs the scene collision bridge in the DogWalk project before leash, snow/ice gameplay, and the ~5k instance expand.

## What Changes

- **Collider Unique** on a scene entity: box / sphere / capsule, plus **static triangle mesh** for world meshes. Body kind **Static**, **Kinematic**, or **Area** (query-only). **Collision layer** and **collision mask** are 32-bit. SI metres; kernel stores Q32.32. No Unreal centimetre scale.
- **Static trimesh only.** Moving bodies use box/sphere/capsule. Dynamic/hollow trimesh is rejected (no collision), not simulated.
- **Missing trimesh:** skip that mesh (no collision). No auto-box. Import does not hard-fail.
- **Object groups** (string names, e.g. `TerrainIce` / `TerrainSnow` / `LeashPivots`) on the entity. Rays/shapecasts/character slides **filter the bitmask**. Groups are read **after** the hit. Areas are queryable (Godot `collide_with_areas`).
- **Physics queries** on the same World the scene bridge owns: ray, sphere/capsule shapecast, and box shapecast. Query shapes are primitives; they may **hit** trimesh; they are not trimesh themselves.
- **Character Controller Unique** + C# `MoveAndSlide`: capsule sweep + slide / floor stick (Godot `move_and_slide` / Unity CCT size). Not Unreal `UCharacterMovementComponent`. Walk does not teleport through mesh by writing `Object.Position`.
- **Play / `engine_player` required.** Pause skips physics steps with Behaviour Tick.
- **Editor Viewport collision wireframe** (authorship overlay). **Edit-mode rays** use the same query API as Play.
- **DogWalk project** (`E:\Blunder Projects\DogWalk`) is the product scene. Test / Sponza are not the main scene for this change.

**Out of scope:** SE-world ~5k `instance_asset_id` expand; dual-character / leash / camera gameplay; heightfields; convex-hull Unique; CCD / joints / island sleep; Unreal CMC; Unreal cm; auto-box fallback; `.tscn` import; Physics Messages; treating groups as layers or layers as groups.

## User stories

1. A static mesh can carry a static triangle-mesh collider; if that mesh has no trimesh data, it does not collide.
2. An entity can take a Unique box, sphere, or capsule collider; anything that moves does not use trimesh.
3. C# and the scene can raycast and sphere/capsule shapecast (box shapecast included), filter by layer mask, and read groups on the hit.
4. The character moves with capsule sweep + slide / floor stick; writing `Object.Position` is not the walk path and does not tunnel through the world.
5. The Viewport shows collision wireframes; edit-mode rays use the same query path as Play.
6. Work lands in the DogWalk project; Test and Sponza are not the main scene.

## Capabilities

### New Capabilities

- `scene-collider-component`: Collider Unique (box / sphere / capsule / static trimesh), Static / Kinematic / Area, layer + mask, metre sizes, scene round-trip
- `scene-object-groups`: string groups on the entity; find-by-group; hit exposes groups
- `physics-trimesh`: Kernel static triangle-mesh collider; no dynamic hollow trimesh; missing data is skip
- `physics-query`: Ray / box / sphere / capsule shapecast against the scene World; mask filter; Areas optional; Play and edit-mode share the API
- `scene-character-controller`: Character Controller Unique; capsule sweep + slide / floor stick; C# `MoveAndSlide`
- `csharp-physics-api`: Blunder.Api + C-ABI for queries, groups, Character Controller
- `collision-viewport`: Editor Viewport collision wireframe overlay; not drawn in Player

### Modified Capabilities

- `inspector-add-menu`: Collider and Character Controller are Unique Add… kinds
- `inspector-present-only-sections`: Collider and Character Controller sections are present-only
- `inspector-add-kind-icons`: kind icons for Collider and Character Controller
- `hierarchy-row-icons`: row icons for present Collider and Character Controller
- `scene-edit-commands`: Add/Remove and property edits for Collider and Character Controller are Document History Commands
- `engine-c-abi`: query / group / Character Controller entry points; ABI version ≥ 13
- `script-native-abi`: NativeAbi completeness includes those pointers
- `play-player`: Player hosts the scene physics World; Pause skips physics steps
- `editor-overlays`: collision wireframe is authorship chrome; Player does not draw or hit-test it
- `asset-import`: glTF collision extras attach static trimesh when present; missing extras skip (no auto-box, no import abort)

## Impact

- Kernel: `PhysicsWorld` static trimesh attach + Q32.32 ray/shapecast; layer bits on colliders; query-only Area colliders
- Runtime: enable physics host (`PhysicsManager` / `RuntimeGlobalContext`); SceneInstance Collider + Character Controller Unique; entity groups; fixed-step accumulator in Play
- Editor: Inspector Add… / sections / icons / Hierarchy icons / Commands; Viewport collision overlay; edit-mode queries
- Script: `engine_c_abi.h` v13, `Blunder.Api` `Physics` / groups / `CharacterController`
- Content: DogWalk project scenes/Behaviours; engine tests stay synthetic (no Test/Sponza as the human scene)
- Docs: CONTEXT Physics + Unique terms; [ADR 0057](../../../docs/adr/0057-scene-collision-bridge.md)
- Depends on: Physics Kernel v0 (implemented). Does not wait on forest instance expand.
