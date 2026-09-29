## 1. COL bake

- [x] 1.1 Extract triangles from glTF COL mesh primitives (Y-up → Z-up)
- [x] 1.2 Flatten emits COL-* Static triangleMesh Uniques (no MeshRenderer)
- [x] 1.3 Skip `*detection*` COL nodes
- [x] 1.4 Runtime importer attaches COL colliders the same way
- [x] 1.5 Update se_world_flatten_test for bake + import
- [ ] 1.6 Rebake DogWalk `se-world.scene.asset` (local content; collider count ≠ 0)

## 2. Sphere CCT (slice B+)

- [x] 2.1 CharacterController Unique sphere mode + local offset
- [x] 2.2 Sphere × trimesh continuous sweep (inflated mesh cast)
- [x] 2.3 Host tests: thin plate rest + mild slope

## 3. Product (C–E)

- [x] 3.1 C# ChocomelLocomotion (input + gravity when airborne + MoveAndSlide; no jump)
- [x] 3.2 Editor View → Collision Debug (default off); CCT wire uses shape offset
- [x] 3.3 Wire Chocomel into se-world + update root.scene.asset Walker to sphere CCT

