## ADDED Requirements

### Requirement: Forest static meshes use existing GPU-driven rendering
Static opaque and alpha-clip MeshRenderers in the DogWalk forest Scene Asset SHALL submit through GPU-driven rendering when those meshes’ Finals contain Meshlets, same as other static props. This change SHALL NOT add a MultiMesh Unique, a second instance table, or a Sponza-only switch for the forest.

#### Scenario: Forest is not MultiMesh Unique
- **WHEN** the editor Viewport draws the baked SE-world forest
- **THEN** static MeshRenderers with Meshlets use GPU-driven rendering
- **AND** no MultiMesh Unique is required for those instances to appear

#### Scenario: CPU cap does not define forest success
- **WHEN** more than 256 static forest MeshRenderers are in view
- **THEN** GPU-driven static casters are not truncated by the Forward mesh draw cap
