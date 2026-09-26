## Requirement: COL-* bake to static trimesh

The se-world flatten bake and runtime glTF attach SHALL emit entities for nodes whose display name starts with `COL-`, with a Static `triangleMesh` Collider Unique whose triangles come from that node's mesh POSITION data (engine Z-up). Those entities SHALL NOT receive a MeshRenderer. Nodes whose name contains `detection` (case-insensitive) SHALL be omitted. Missing or empty triangle data SHALL skip that node (no auto-box).

### Scenario: Set COL and GEO

- **WHEN** a set glTF contains `COL-ground` and `GEO-ground` with triangle meshes
- **THEN** bake emits `COL-ground` with a non-empty static triangleMesh collider and no mesh GUID, and emits `GEO-ground` with a Mesh Asset GUID as before

### Scenario: Detection COL omitted

- **WHEN** a COL node name contains `detection`
- **THEN** bake and import omit that entity
