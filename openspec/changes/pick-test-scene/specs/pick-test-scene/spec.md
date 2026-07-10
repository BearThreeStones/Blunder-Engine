## ADDED Requirements

### Requirement: Pick-test cube mesh asset

The project SHALL provide a pick-test cube mesh at virtual path `assets/Meshes/Cube.mesh.yaml` backed by a glTF 2.0 source under `resources/Models/PickTest/`, importable by `AssetManager::loadMesh` without Draco or unsupported topology.

#### Scenario: Mesh loads in editor

- **WHEN** the editor starts and cooks assets
- **THEN** `AssetManager::loadMesh("assets/Meshes/Cube.mesh.yaml")` SHALL return a non-null mesh with triangle primitives suitable for GPU pick prepass

#### Scenario: Descriptor follows content layout

- **WHEN** the cube descriptor is inspected on disk
- **THEN** it SHALL use `type: Mesh`, a unique `guid`, `source: resources/Models/PickTest/Cube.gltf`, and `import` options consistent with other project mesh YAML files

### Requirement: Pick-test scene with three overlapping root boxes

The project SHALL provide `assets/Scenes/pick_test.scene.asset` containing exactly three root-level entities (`BoxFront`, `BoxMid`, `BoxBack`) that each reference `assets/Meshes/Cube.mesh.yaml`, with no parent entity and no child scene references, positioned so the default editor camera ray through the viewport center intersects all three boxes in depth order.

#### Scenario: Three independent pickable roots

- **WHEN** `pick_test.scene.asset` is loaded
- **THEN** the active scene instance SHALL contain three mesh renderers on three distinct root entities named `BoxFront`, `BoxMid`, and `BoxBack`

#### Scenario: Depth stacking along view axis

- **WHEN** the scene is loaded and the editor camera frames the box cluster
- **THEN** `BoxFront` SHALL be nearest along the primary view ray, `BoxMid` behind it, and `BoxBack` farthest, with pairwise center separation between 0.8m and 2.0m on the engine Y axis (forward)

#### Scenario: No hierarchy parent dedupe trap

- **WHEN** a GPU multi-hit pick is performed at the overlapping pixel
- **THEN** promoted entity ids SHALL remain three distinct roots (no shared parent), enabling at least three piercing-menu rows and three same-pixel cycle steps

### Requirement: Default scene stripped for non-pick clutter

The default `assets/Scenes/root.scene.asset` SHALL NOT reference Sponza or nested sub-scenes used only for rendering demos, so accidental startup on `root` does not load heavy geometry.

#### Scenario: Root scene is minimal

- **WHEN** `root.scene.asset` is loaded
- **THEN** it SHALL NOT contain `Sponza.mesh` or `childScenes` entries

### Requirement: Editor startup uses pick-test scene

The editor SHALL load `assets/Scenes/pick_test.scene.asset` on startup by default so pick QA does not require manual scene open.

#### Scenario: Cold start scene

- **WHEN** `engine_editor` launches without an explicit scene override
- **THEN** the active scene path SHALL be `assets/Scenes/pick_test.scene.asset`

#### Scenario: Optional scene override

- **WHEN** environment variable `BLUNDER_STARTUP_SCENE` is set to a valid `assets/.../*.scene.asset` virtual path
- **THEN** the editor SHALL load that scene instead of the default pick-test scene

### Requirement: Pick-test QA documentation

Agent documentation SHALL describe the pick-test scene layout and expected manual outcomes for hybrid pick QA (piercing menu row count, click cycling, parent promotion on a separate hierarchy fixture if documented).

#### Scenario: Doc references scene

- **WHEN** an agent reads the render-pipeline or testing pick QA section
- **THEN** it SHALL find the virtual path `assets/Scenes/pick_test.scene.asset`, entity names, and expected ≥3 multi-hit results at the stack pixel
