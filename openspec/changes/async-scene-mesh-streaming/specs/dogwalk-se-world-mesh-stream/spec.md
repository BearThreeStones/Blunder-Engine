# Spec Delta

## Purpose

Accept unique-mesh streaming on the DogWalk forest Scene Asset so a human can Iterate the editor and Player before 252 unique Mesh Assets finish, then recognize the metre Z-up forest when residency completes.

## ADDED Requirements

### Requirement: Acceptance scene is DogWalk se-world
Human acceptance for this change SHALL open DogWalk `Assets/Scenes/se-world.scene.asset` (virtual path `assets/Scenes/se-world.scene.asset`) on an editor / Player build that binds Mesh Assets by GUID. Test Project scenes and Sponza SHALL NOT stand in as this forest. Collision QC SHALL remain on `Assets/Scenes/root.scene.asset`.

#### Scenario: Forest file is the walk target
- **WHEN** the human walks this change
- **THEN** the Live document or Play entry is DogWalk `se-world.scene.asset`
- **AND** it is not a Test or Sponza scene
- **AND** it is not collision QC `root.scene.asset`

### Requirement: GUID-bind editor, not the 10k glTF collision exe
The walk SHALL use the product path that attaches unique Mesh Asset GUIDs (shared library meshes). It SHALL NOT use the collision product executable that re-imports about 10k glTF documents per instance.

#### Scenario: Open is GUID bind
- **WHEN** the human opens the forest Scene Asset for this change
- **THEN** unique Mesh residency is per Mesh Asset GUID
- **AND** the session is not the collision-branch ~10k glTF reimport binary

### Requirement: First editor frame is interactive before 252 unique meshes finish
Opening that forest SHALL let the editor shell and Viewport Iterate (orbit / click) before all ~252 unique Mesh Assets are CPU-resident and GPU-resident. A first frame that waits on the full unique set SHALL fail this requirement. This slice SHALL NOT require a numeric first-frame millisecond cap.

#### Scenario: Editor can turn before the forest is full
- **WHEN** the author opens `assets/Scenes/se-world.scene.asset` in DogWalk
- **THEN** the editor window can orbit the Viewport and click UI on the first Iterate
- **AND** unique Mesh GPU residency MAY still be in progress

### Requirement: Forest grows to real trees, fence, and GEO
As unique meshes become resident, the Viewport SHALL fill with SE-world’s real trees, fence, and ground GEO (including grass / bush / reed in that suite). Success SHALL NOT be eight empty instance nodes or placeholder boxes. Units SHALL be metres, Z-up.

#### Scenario: Complete forest is recognizable
- **WHEN** unique Mesh residency for the open forest has finished
- **THEN** the Viewport shows textured library trees, fence, and set GEO ground
- **AND** the scale is metres, not centimetre Sponza

### Requirement: Play streams the same forest
`engine_player` SHALL load the same DogWalk forest Scene Asset. Its first Iterate SHALL NOT be gated on all unique Mesh CPU+GPU residency. When residency is complete, the Player SHALL draw that forest through existing GPU-driven submission. Play SHALL NOT require C# Find / group / ray, collision wireframe, or the dog standing on this ground.

#### Scenario: Play is not a long black stall then a pop
- **WHEN** Play starts on DogWalk `assets/Scenes/se-world.scene.asset`
- **THEN** the Player process reaches Iterate before every unique Mesh is GPU-resident
- **AND** after residency completes, the Player view shows the same class of trees, fence, and GEO as the editor Viewport

### Requirement: One failed unique Mesh does not empty the forest
If one unique Mesh Asset in the forest fails, only MeshRenderers of that GUID SHALL be missing. The rest of the forest SHALL remain.

#### Scenario: One library mesh missing
- **WHEN** a single unique Mesh GUID in `se-world.scene.asset` fails to load
- **THEN** that class is absent
- **AND** other resident unique meshes still draw

### Requirement: Checkerboard albedo is not a failure
First-frame or in-progress Bindless fallback / checkerboard albedo from the Texture Loader SHALL NOT fail this acceptance. Waiting until `textureUploadInFlightCount()==0` is QC wait, not the product open gate.

#### Scenario: Fallback textures while meshes stream
- **WHEN** unique meshes appear before color textures are GPU-resident
- **THEN** checkerboard or Bindless fallback albedo is allowed
- **AND** the human walk still passes this change
