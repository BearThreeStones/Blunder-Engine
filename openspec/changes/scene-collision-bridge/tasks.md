# Tasks

## 1. Kernel static trimesh

- [x] 1.1 Add Static triangle-mesh Collider attach on `PhysicsWorld` (SI metres, Q32.32) and verify `physics_trimesh_test` (or golden) rests a capsule/sphere on a one-triangle floor
- [x] 1.2 Reject Dynamic/Kinematic/Area trimesh attach (no kernel shape) and verify a test that the body stays usable with no mesh collider
- [x] 1.3 Empty triangle list attaches nothing (no AABB box) and verify a skip test; existing `physics_golden_suite` still passes

## 2. Kernel queries, layers, Areas

- [x] 2.1 Add layer + mask bits on colliders (default layer bit 0, mask all) and verify mask-miss vs mask-hit tests
- [x] 2.2 Add Area/query-only colliders that generate no solid contacts and verify a Dynamic or CCT is not blocked by an overlapping Area
- [x] 2.3 Add raycast + box/sphere/capsule shapecast in Q32.32; primitive queries may hit trimesh; verify `physics_query_test` covers ray, each shapecast, trimesh hit, and rejected trimesh query shape
- [x] 2.4 Collide-with-areas defaults false; verify Area miss/hit tests

## 3. Collider Unique and groups

- [x] 3.1 Add Collider Unique on `SceneInstance` (shape, body kind Static/Kinematic/Area, metre sizes, layer, mask) with scene JSON round-trip; verify serialize/load test
- [x] 3.2 Identity float→fixed conversion (no ×100); verify a 1 m box is 1 m in the World
- [x] 3.3 Entity `groups` string list round-trips; hits include groups; verify group serialize + hit payload test
- [x] 3.4 Triangle Mesh Unique on non-Static body skips kernel collider; verify no-auto-box

## 4. Physics host

- [x] 4.1 Enable a physics host on `RuntimeGlobalContext` with one `PhysicsWorld` per `SceneInstance`; rebuild colliders from Unique + TRS; verify instantiate creates kernel shapes
- [x] 4.2 Play accumulator `dt = 1/60` calls `step`; Pause skips steps; verify a Player/host test or headless Play step count
- [x] 4.3 Edit mode does not step Dynamics but keeps poses in the World for queries; verify an edit-mode query test after TRS commit

## 5. Character Controller

- [x] 5.1 Add Character Controller Unique (capsule +Z, radius/height metres, slope, step, snap, skin, mask) with scene round-trip; verify serialize test
- [x] 5.2 Implement sweep + slide + floor snap (`MoveAndSlide`); Areas do not block; verify wall-slide and floor tests
- [x] 5.3 `Object.Position` write remains teleport; verify a teleport-through-wall test that sweep does not run

## 6. C-ABI and Blunder.Api

- [x] 6.1 Add C-ABI query/group/CCT entries; bump `BLUNDER_ENGINE_C_ABI_VERSION` to ≥ 13; verify native ABI test without script host
- [x] 6.2 Extend `BlunderNativeAbi` completeness; verify managed completeness test requires the new pointers
- [x] 6.3 Add `Blunder.Api` Physics + groups + CharacterController façades through registered pointers; verify a managed or native-abi test that MoveAndSlide and Raycast do not DllImport a second image

## 7. Inspector, History, icons

- [x] 7.1 Add… Collider (default Static Box) and Character Controller; Unique-once disable; no Object materialization; present-only Inspector sections; verify `inspector_add_menu` tests
- [x] 7.2 Document History Commands for Add/Remove/field edits; verify undo/redo tests
- [x] 7.3 Add… kind icons and Hierarchy row icons for Collider and Character Controller; verify icon tests

## 8. Viewport overlay and edit queries

- [x] 8.1 Editor Overlay collision wireframes for Collider + CCT (box/sphere/capsule/trimesh edges); Player draws none; verify overlay enabled-in-editor / disabled-in-player tests
- [x] 8.2 Edit-mode rays call the same physics query API as Play (not GPU pick); verify an edit query test shares the kernel query helper

## 9. Import skip and DogWalk fixtures

- [x] 9.1 glTF collision extras (`collision_info` or the Godot exporter key confirmed at apply) attach Static trimesh when usable; missing/empty extras skip, no auto-box, no import abort; verify import tests for present and missing extras
- [x] 9.2 DogWalk Project fixture scene: static trimesh or primitive floor, one Area with group `TerrainIce`, one Character Controller, a Behaviour that rays (mask + collide-with-areas) and MoveAndSlides; do not retarget Test/Sponza as the main scene; verify the scene file exists under DogWalk and a synthetic engine test still covers the behaviours

## 10. Docs closeout

- [x] 10.1 Keep `CONTEXT.md` Physics / Unique terms aligned with implementation names; ADR 0057 stays the product-split record
- [x] 10.2 `openspec validate scene-collision-bridge` passes
