# Spec Delta

## Purpose

Bake Godot SE-world `instance_asset_id` extras into one flat Scene Asset so Play and the editor load real library meshes without a runtime instance map or nested scenes.

## ADDED Requirements

### Requirement: Offline bake, not runtime lookup
The product forest SHALL be produced by an offline bake that reads Godot `SE-world.gltf`, `asset_index.json`, and reachable SE/SL set and library glTFs, then writes one flat Scene Asset. Loading that Scene Asset SHALL instantiate entities and attach meshes from Mesh Asset references. Load SHALL NOT look up `instance_asset_id`, SHALL NOT consult `asset_index.json`, and SHALL NOT instantiate nested Scene Assets.

#### Scenario: Baked scene loads without asset index
- **WHEN** the Player or editor loads the baked forest Scene Asset and no `asset_index.json` is on the load path
- **THEN** layout instances still instantiate
- **AND** their meshes attach from Mesh Asset references already stored on those entities

#### Scenario: Runtime ignores instance extras
- **WHEN** a glTF node still carries `instance_asset_id` extras
- **AND** that glTF is imported or attached at runtime (not the bake)
- **THEN** those extras are not expanded into child Scene Assets or a second prefab graph

### Requirement: Layout instances are entities with shared meshes
The bake SHALL emit one scene entity per reachable SE/SL layout instance (LI/PR), excluding ikea, zoo, and vertical_slice. Each such entity SHALL reference a shared Mesh Asset GUID for its library glTF, not a duplicated mesh body per instance. Set nodes MAY parent those entities for grouping. `parent` SHALL be an entity-name grouping string, not a nested Scene Asset.

#### Scenario: Two instances share one Mesh Asset
- **WHEN** two layout instances use the same library asset id
- **THEN** both scene entities store the same Mesh Asset GUID
- **AND** the Project does not register a second Mesh Asset solely because the second instance exists

#### Scenario: Parent is grouping, not a child scene
- **WHEN** a layout instance entity records parent `SL-hub-trees`
- **THEN** instantiate parents that entity under the grouping entity of that name
- **AND** the Scene Asset has no `childScenes` composition

### Requirement: Nested library instances expand under the layout entity
When a library glTF itself carries `instance_asset_id` (needles, knots, leaves, and the same class of patch), the bake SHALL expand those instances as entities under that layout instance. A layout tree that only stored the outer instance id SHALL NOT be accepted as complete.

#### Scenario: Tree internals are in the scene
- **WHEN** a library tree glTF lists nested `instance_asset_id` children that resolve in `asset_index.json`
- **THEN** the baked Scene Asset contains child entities under that layout instance for those nested assets
- **AND** those children reference their library Mesh Assets

### Requirement: Unique entity names
Every entity in the baked Scene Asset SHALL have a scene-unique name. Colliding Godot node names SHALL be uniquified as `stem` then `stem_N` (N starting at 1). Scene instantiate and mesh attach SHALL address entities by those unique names.

#### Scenario: Duplicate stems become unique
- **WHEN** the Godot source has more than one node with the same display name
- **THEN** the baked Scene Asset keeps the first as `stem` and further copies as `stem_1`, `stem_2`, …
- **AND** no two entities share a name

### Requirement: Missing asset ids skip, bake continues
When a layout or nested instance’s asset id is absent from `asset_index.json` or the referenced glTF file is missing, the bake SHALL skip that instance, SHALL log the skip, and SHALL continue. The bake SHALL NOT abort into an empty Scene Asset solely because some instances failed. The known missing id `9c53197a476fa552` (`LI-bushlet_blue_delicate_006`, eight placements) SHALL skip.

#### Scenario: Eight missing bushlets do not empty the scene
- **WHEN** bake encounters eight instances whose asset id is `9c53197a476fa552`
- **THEN** those eight instances are omitted
- **AND** the remaining reachable instances are still written to the Scene Asset

#### Scenario: Missing file skips one instance
- **WHEN** an otherwise valid asset id points at a glTF path that does not exist
- **THEN** that instance is skipped
- **AND** later instances still bake

### Requirement: COL-* have no MeshRenderer
Nodes whose display name starts with `COL-` SHALL NOT receive a MeshRenderer this slice. Visible set geometry SHALL come from GEO (and the same class of non-COL mesh). The bake SHALL NOT treat COL meshes as a placeholder box or as the visible ground.

#### Scenario: Collision mesh is not drawn
- **WHEN** a set glTF contains both a `COL-*` node and a `GEO-*` node
- **THEN** the GEO node can receive a MeshRenderer
- **AND** the COL node has no MeshRenderer

### Requirement: Metres, Z-up, keep negative scale
Baked entity TRS SHALL be engine metres, Z-up, using the existing glTF→engine basis `(x, y, z)_gltf → (x, z, −y)_engine`. The bake SHALL NOT apply Sponza centimetre scale (`0.008`). The bake SHALL NOT treat forest translations whose absolute value exceeds 64 as a centimetre world. Negative scale on a source instance SHALL be preserved.

#### Scenario: Creek-side layout stays tens of metres
- **WHEN** a source layout translation is on the order of tens of metres (for example x ≈ −47, z ≈ 70 in the Godot file)
- **THEN** the baked entity translation is the engine-basis image of that metre translation
- **AND** it is not multiplied by `0.008`

#### Scenario: Negative scale is kept
- **WHEN** a source instance has scale −1 on an axis
- **THEN** the baked entity keeps a negative scale on the corresponding engine axis
