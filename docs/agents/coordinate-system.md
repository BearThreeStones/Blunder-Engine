# Coordinate System

Blunder world space is **right-handed, Z-up**:

| Axis | Direction |
|------|-----------|
| +X | Right |
| +Y | Forward |
| +Z | Up |

Constants and glTF import helpers live in
[`coordinate_system.h`](../../engine/src/runtime/core/math/coordinate_system.h).
`Transform` local axes match world (+Y forward, +Z up). The editor ground grid
uses the **XY** plane (`ForwardGridPlane::xy`).

**glTF** sources are Y-up (Khronos default). Runtime import applies a fixed basis
change `(x, y, z) → (x, z, -y)` to mesh vertices/normals/tangents and
`similarityGltfToEngine()` on scene node matrices. Raw `.gltf` / `.glb` on disk
are unchanged; re-cook meshes after changing this mapping
(`asset_compiler --force` or delete `.blunder/cooked/`).

**`.scene.asset`** positions and `euler_degrees` are authored in engine Z-up
(fixed-axis ZYX: rotations about +X, +Y, +Z).

## See also

- [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md)
- [render-pipeline.md](render-pipeline.md)
- [golden-principles.md](../golden-principles.md)
- [coordinate_system.h](../../engine/src/runtime/core/math/coordinate_system.h)
