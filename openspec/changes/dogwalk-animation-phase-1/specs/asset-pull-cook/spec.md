## MODIFIED Requirements

### Requirement: Fast Path
When Final is missing or stale, the system SHALL load Intermediate data into a Loaded Asset without blocking the first preview on Cook completion. For mesh Assets, Intermediate SHALL be glTF/GLB after successful Import. Fast Path and Final MAY coexist in one Editor Session. For skinned meshes, Fast Path SHALL use CPU skinning per `animation-skinning`.

#### Scenario: Stale cooked falls back to Intermediate glTF
- **WHEN** cooked meta indicates Intermediate or descriptor is newer than Final and Intermediate `source` is glTF/GLB
- **THEN** load uses Intermediate glTF (Fast Path) and treats Final as stale

#### Scenario: Fast Path skinned preview
- **WHEN** Final is missing for a skinned mesh and the asset is previewed
- **THEN** Fast Path loads Intermediate glTF and CPU-skins from Skeleton pose

### Requirement: Minimal Asset Dependency Graph
The system SHALL maintain a minimal Asset Dependency Graph with edges: Scene Asset → Mesh Asset; Mesh Asset → Texture Asset when an explicit Texture Asset Reference exists in the mesh descriptor (`texture_guids`); Scene or AnimationPlayer consumers → AnimationClip Assets when referenced by GUID; each Asset → its Intermediate inputs (descriptor and Intermediate `source` / YAML file) as freshness leaves. Embedded image URIs that are not registered Texture Assets SHALL NOT appear as graph nodes. Material Asset nodes are out of v1. COLLADA `<image>` URI rules are obsolete with COLLADA Intermediate removal.

#### Scenario: Texture Intermediate change invalidates dependent Mesh Final
- **WHEN** an Intermediate texture file changes for a Texture Asset referenced by a Mesh Asset via `texture_guids`
- **THEN** that Texture’s Final and the dependent Mesh’s Final are marked stale

#### Scenario: Scene depends on Mesh
- **WHEN** a Scene Asset references a Mesh Asset by GUID
- **THEN** the dependency graph contains a Scene→Mesh edge used for invalidation

#### Scenario: Clip Intermediate change invalidates clip Final
- **WHEN** an AnimationClip YAML Intermediate changes on disk
- **THEN** that Clip Asset’s Final is marked stale

### Requirement: Asset Watch invalidation
The editor SHALL watch the Assets root and Intermediate data under the Resources root (excluding the Source root for Intermediate invalidation). Changes SHALL invalidate Finals for affected Assets and dependents via the dependency graph.

#### Scenario: Descriptor change invalidates Final
- **WHEN** a mesh, texture, or AnimationClip Asset Descriptor file changes on disk
- **THEN** that Asset’s Final is marked stale

#### Scenario: Intermediate Resources change invalidates Final
- **WHEN** an Intermediate glTF/GLB, AnimationClip YAML, or image referenced by an Asset changes on disk
- **THEN** that Asset’s Final is marked stale

## REMOVED Requirements

### Requirement: Intermediate Upgrade from legacy glTF
**Reason:** ADR 0019 restores glTF as mesh Intermediate and removes COLLADA; upgrading glTF→COLLADA is obsolete and harmful.
**Migration:** Do not convert glTF Intermediate to `.dae`. Remaining `.dae` Intermediate Assets migrate GUID-preserving back to glTF or reimport from archived Source (see design migration plan).

#### Scenario: No glTF-to-COLLADA upgrade on scan
- **WHEN** registry scan finds a mesh Asset whose Intermediate `source` is glTF/GLB
- **THEN** the system does not convert it to COLLADA or rewrite `source` to `.dae`
