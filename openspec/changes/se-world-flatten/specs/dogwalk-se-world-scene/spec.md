# Spec Delta

## Purpose

Put the baked SE-world forest in the DogWalk Project as the main scene so Viewport and Play show real trees, fence, and ground instead of Test, Sponza, or the collision QC fixture.

## ADDED Requirements

### Requirement: DogWalk main scene is se-world
The DogWalk Project’s main forest scene SHALL be `Assets/Scenes/se-world.scene.asset` (virtual path `assets/Scenes/se-world.scene.asset`). Opening DogWalk for this change SHALL load that Scene Asset as the Live document when `--scene` is omitted and no Editor Session restore GUID overrides it. Test Project scenes and Sponza SHALL NOT be this forest.

#### Scenario: DogWalk opens the forest
- **WHEN** the author opens the DogWalk Project with no `--scene` and no remembered Scene Asset GUID
- **AND** `assets/Scenes/se-world.scene.asset` exists
- **THEN** the Live document is that forest Scene Asset
- **AND** it is not a Test or Sponza scene

#### Scenario: Explicit scene still wins
- **WHEN** the editor is started with `--scene` pointing at another Scene Asset in DogWalk
- **THEN** that Scene Asset is the Live document

### Requirement: Collision QC scene is left alone
The bake SHALL NOT overwrite DogWalk `Assets/Scenes/root.scene.asset`. Collision QC fixtures already in that file SHALL remain loadable as their own scene.

#### Scenario: root.scene.asset still exists
- **WHEN** the flatten bake writes `se-world.scene.asset`
- **THEN** `Assets/Scenes/root.scene.asset` is still on disk
- **AND** its prior entities are unchanged by this bake

### Requirement: Viewport shows real SE-world meshes
When the forest Scene Asset is Live, the editor Viewport SHALL draw reachable SE/SL layout instances and set GEO (trees, fence, ground, and grass / bush / reed in that suite) from imported library and set Mesh Assets. The Viewport SHALL NOT satisfy this requirement with eight empty instance nodes, with placeholder boxes, or with the Test Chocomel ground plane.

#### Scenario: Trees and ground are visible
- **WHEN** the author opens `assets/Scenes/se-world.scene.asset` in the DogWalk Project
- **THEN** the Viewport shows textured library trees, fence, and set GEO ground
- **AND** the drawn instance count is on the order of the reachable layout (~4795, minus skipped missing ids), not a handful of boxes

### Requirement: Play draws the same forest
`engine_player` SHALL load `assets/Scenes/se-world.scene.asset` as a Play entry and draw those MeshRenderers. Play SHALL NOT require a C# Find, group query, or raycast to spawn or draw the forest. Play SHALL NOT require the dog to stand on this ground this slice.

#### Scenario: Play shows the forest without Find
- **WHEN** Play starts on the DogWalk forest Scene Asset
- **THEN** the Player view draws the same class of trees, fence, and GEO ground as the editor Viewport
- **AND** no Behaviour Find/group/ray is required for those meshes to appear

### Requirement: Counts and suites
Reachable content SHALL be the SE/SL suite only: layout instances from `SE-world.gltf` plus set glTFs under hub, fence, clearing, and world. `SE-asset_ikea`, `SE-asset_zoo`, `vertical_slice`, and `world.tscn` nodes outside SE-world SHALL NOT enter this Scene Asset. Hierarchy row count MAY exceed the layout-instance count because nested library instances and set GEO are extra entities.

#### Scenario: Ikea and zoo stay out
- **WHEN** the bake runs against the Godot project that also contains ikea and zoo suites
- **THEN** the baked Scene Asset contains no entities sourced from `SE-asset_ikea` or `SE-asset_zoo`

#### Scenario: Layout count is the acceptance grain
- **WHEN** the bake finishes with only the known missing bushlet id skipped
- **THEN** layout-instance entities are about 4795 minus those eight
- **AND** Hierarchy MAY list more rows than that layout count
