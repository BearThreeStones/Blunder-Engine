# Spec Delta

## Purpose

Load unique Mesh Assets for any open Scene Asset without stalling the first Iterate on CPU mesh reads or a full unique-set `GpuMesh` upload, while entities exist immediately and missing meshes skip instead of emptying the scene.

## ADDED Requirements

### Requirement: Entity table is complete before first Iterate
`SceneSystem::loadScene` and editor `openScene` SHALL deserialize the Scene Asset and instantiate its entity table (including parent grouping, cameras, and unique names) before the process returns to Iterate. They SHALL NOT page entities by chunk or distance. They SHALL NOT require a second PackedScene or nested Scene Asset to open.

#### Scenario: First frame has entities
- **WHEN** the author opens a Scene Asset that lists many entities with Mesh Asset GUID refs
- **THEN** the entity table for that document exists before the first Iterate
- **AND** Hierarchy can list those entities (folded roots allowed)
- **AND** the process does not wait until every unique Mesh Asset is GPU-resident before that Iterate

### Requirement: Unique Mesh CPU work is Jobs
Unique Mesh Asset CPU residency (cooked `.meshbin` / Fast Path into Mesh data) SHALL run as Jobs over Job data. Those Jobs SHALL NOT call RHI, Object, ClassDB, SceneInstance, or Slint. The tick SHALL NOT enter a Job barrier in order to show a frame that skips not-yet-resident meshes.

#### Scenario: CPU read does not freeze Iterate
- **WHEN** a scene with several unique Mesh Assets opens
- **THEN** those unique meshes are read as Jobs
- **AND** Iterate continues while those Jobs are outstanding
- **AND** the tick does not wait on a Job barrier for those reads before presenting

### Requirement: Unique GpuMesh upload is tick-budgeted
After a unique Mesh Asset is CPU-resident, its `GpuMesh` SHALL be created on the RHI owner thread with a per-tick budget. One scene-to-render sync SHALL NOT synchronously upload every unique mesh in the open scene. Jobs SHALL NOT create `GpuMesh` objects.

#### Scenario: First sync does not upload the whole unique set
- **WHEN** a scene with many unique Mesh Assets reaches the first scene-to-render sync
- **THEN** that sync does not `GpuMesh::create` every unique mesh in the scene
- **AND** remaining unique meshes upload on later ticks within the budget

### Requirement: Draws skip until that GUID has a GpuMesh
A MeshRenderer SHALL bind its Mesh Asset when that unique GUID is CPU-resident. Until that GUID has a resident `GpuMesh`, Viewport and Player SHALL skip that MeshRenderer. They SHALL NOT draw placeholder boxes for that hole. They SHALL NOT drop the rest of the scene because one GUID is not ready.

#### Scenario: Forest grows as meshes resident
- **WHEN** unique Mesh Assets become GPU-resident over successive ticks
- **THEN** MeshRenderers for those GUIDs appear
- **AND** MeshRenderers whose GUID is not yet resident are absent that frame
- **AND** the scene instance remains loaded

### Requirement: In-flight unique meshes coalesce by GUID
While a Mesh Asset GUID is already in flight (CPU Job or GPU upload), a second request for that same GUID SHALL NOT start a second CPU read or a second `GpuMesh` upload.

#### Scenario: Shared GUID uploads once
- **WHEN** many MeshRenderers reference the same Mesh Asset GUID
- **THEN** the engine performs at most one CPU residency Job and at most one `GpuMesh` upload for that GUID

### Requirement: Job order is document order
Unique Mesh Jobs SHALL be submitted in the order those GUIDs first appear in the Scene Asset document. This slice SHALL NOT reorder by camera frustum or distance.

#### Scenario: First-mentioned GUID is first queued
- **WHEN** a Scene Asset mentions Mesh GUID A then Mesh GUID B
- **THEN** the Mesh Loader queues A before B
- **AND** it does not sort those Jobs by the current view frustum

### Requirement: Failed unique Mesh skips that class
When a unique Mesh Asset fails to become CPU-resident or GPU-resident, MeshRenderers that reference only that GUID SHALL not draw. Other MeshRenderers SHALL continue. Load of the Scene Asset SHALL NOT abort into an empty instance solely because one unique Mesh failed.

#### Scenario: One missing library mesh
- **WHEN** one unique Mesh Asset GUID in the open scene cannot load
- **THEN** renderers for that GUID are skipped
- **AND** renderers for other unique GUIDs still draw when resident
- **AND** the entity table stays

### Requirement: Cancel on scene drop
When the live scene is dropped or replaced while unique Mesh requests are in flight, the process SHALL remain stable. Completions for dropped requests SHALL NOT bind MeshRenderers or keep `GpuMesh` objects that belong to the dropped scene.

#### Scenario: Scene switch drops in-flight meshes
- **WHEN** the author switches scenes while unique Mesh Jobs or GPU uploads are in flight
- **THEN** the process SHALL NOT crash
- **AND** a completion for the old scene SHALL NOT install a `GpuMesh` that was dropped with that scene

### Requirement: Headless without a device skips GPU upload
When the process has no Vulkan device, the Mesh Loader SHALL NOT create `GpuMesh` objects. Headless Editor and Headless Player SHALL still start and exit. CPU Jobs MAY still run.

#### Scenario: Headless skips GpuMesh
- **WHEN** a Headless Editor or Headless Player starts with no Vulkan device
- **THEN** the Mesh Loader SHALL NOT upload `GpuMesh`
- **AND** the process SHALL exit cleanly

### Requirement: Editor and Player share one residency rule
Editor `openScene` and Play `engine_player` SHALL use the same unique-mesh residency path as `SceneSystem::loadScene`. The editor SHALL NOT wait until all unique meshes are CPU+GPU resident before returning to Iterate. The Player SHALL stream in its own process and SHALL NOT receive Live editor pointers as the Play world.

#### Scenario: Open returns before meshes finish
- **WHEN** the author opens a Scene Asset in the editor
- **THEN** `openScene` returns with the entity table instantiated
- **AND** unique Mesh residency continues on later ticks

### Requirement: No new loading UI and no binary Scene Asset this slice
This slice SHALL NOT add a Slint “loading meshes” banner or progress bar as the product open UX. Geometry appearing as unique meshes resident SHALL be the visible progress. This slice SHALL NOT replace `.scene.asset` JSON with a binary Scene Asset format. This slice SHALL NOT require a hard first-frame millisecond budget as acceptance.

#### Scenario: Geometry pops without a loading bar
- **WHEN** the author opens a large Scene Asset
- **THEN** no new mesh-loading chrome is required for the first Iterate to be usable
- **AND** meshes may appear after that frame

### Requirement: Streaming unit is unique Mesh Assets, not per-instance glTF import
Unique Mesh Asset GUID (or registered Mesh descriptor path) SHALL be the residency unit. This slice SHALL NOT add streaming around per-instance `openGltfImportDocument` / glTF-under-entity import. That importer SHALL remain out of the product open stream.

#### Scenario: Shared GUID is not re-imported per instance
- **WHEN** thousands of MeshRenderers share a few hundred Mesh Asset GUIDs
- **THEN** residency work is per unique GUID
- **AND** the engine does not open a glTF import document once per MeshRenderer to stream the scene

### Requirement: Metres and Z-up are unchanged
Streaming unique Mesh residency SHALL NOT rewrite entity TRS, SHALL NOT apply Sponza `0.008`, and SHALL NOT classify the forest with `kMeterSpaceTranslationMaxAbs`. Negative scale SHALL stay negative.

#### Scenario: Forest stays metres
- **WHEN** unique meshes become resident on a metre Z-up Scene Asset
- **THEN** instance transforms are the authored engine-metre TRS
- **AND** they are not scaled as centimetre Sponza

### Requirement: Texture Loader and material hydrate stay as they are
Color material textures SHALL keep using the Texture Loader. First-frame Bindless fallback / checkerboard albedo SHALL NOT fail this capability. This slice SHALL NOT use 2/frame glTF material hydrate as the scene-open policy.

#### Scenario: Checkerboard albedo is allowed
- **WHEN** unique meshes are GPU-resident but their color textures are still in the Texture Loader
- **THEN** draws MAY sample the Bindless fallback
- **AND** that state is not a Mesh Loader failure
