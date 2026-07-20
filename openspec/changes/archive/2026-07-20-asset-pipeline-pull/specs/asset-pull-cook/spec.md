## ADDED Requirements

### Requirement: Pull pipeline freshness
The editor and runtime SHALL pull Assets by identity. Final Assets SHALL be produced by Cook when missing or stale relative to Intermediate inputs. A full-project Cook MAY run as warm-up or CI, but SHALL NOT be the only freshness mechanism for authoring.

#### Scenario: Load triggers on-demand Cook when Final missing
- **WHEN** an Asset is loaded and no fresh Final exists
- **THEN** the system uses Fast Path Intermediate data for the Loaded Asset and requests Cook for that Asset

#### Scenario: Fresh Final preferred
- **WHEN** an Asset is loaded and a Final exists that matches current Intermediate freshness
- **THEN** the Loaded Asset is built from the Final, not by re-parsing Intermediate as the primary path

### Requirement: Fast Path
When Final is missing or stale, the system SHALL load Intermediate data into a Loaded Asset without blocking the first preview on Cook completion. Fast Path and Final MAY coexist in one Editor Session.

#### Scenario: Stale cooked falls back to Intermediate
- **WHEN** cooked meta indicates Intermediate or descriptor is newer than Final
- **THEN** load uses Intermediate (Fast Path) and treats Final as stale

### Requirement: Minimal Asset Dependency Graph
The system SHALL maintain a minimal Asset Dependency Graph with edges: Scene Asset → Mesh Asset; Mesh Asset → Texture Asset when an explicit Texture Asset Reference exists; each Asset → its Intermediate inputs (descriptor and Intermediate `source` file) as freshness leaves. Embedded glTF textures that are not registered Assets SHALL NOT appear as graph nodes. Material Asset nodes are out of v1.

#### Scenario: Texture Intermediate change invalidates dependent Mesh Final
- **WHEN** an Intermediate texture file changes for a Texture Asset referenced by a Mesh Asset
- **THEN** that Texture’s Final and the dependent Mesh’s Final are marked stale

#### Scenario: Scene depends on Mesh
- **WHEN** a Scene Asset references a Mesh Asset by GUID
- **THEN** the dependency graph contains a Scene→Mesh edge used for invalidation

### Requirement: Asset Watch invalidation
The editor SHALL watch the Assets root and Intermediate data under the Resources root (excluding the Source root for Intermediate invalidation). Changes SHALL invalidate Finals for affected Assets and dependents via the dependency graph.

#### Scenario: Descriptor change invalidates Final
- **WHEN** a mesh or texture Asset Descriptor file changes on disk
- **THEN** that Asset’s Final is marked stale

#### Scenario: Intermediate Resources change invalidates Final
- **WHEN** an Intermediate glTF or image referenced by an Asset changes on disk
- **THEN** that Asset’s Final is marked stale

### Requirement: Source change triggers Reimport
Changes under the Source root for an archived Source file SHALL trigger automatic Reimport for Assets that archive that Source (debounced).

#### Scenario: Archived FBX change reimports
- **WHEN** an archived Source FBX watched under the Source root changes
- **THEN** Reimport runs for the owning Asset, refreshing Intermediate and invalidating Finals
