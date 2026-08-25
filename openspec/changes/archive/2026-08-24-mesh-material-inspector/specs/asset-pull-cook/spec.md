## MODIFIED Requirements

### Requirement: Minimal Asset Dependency Graph
The system SHALL maintain a minimal Asset Dependency Graph with edges: Scene Asset → Mesh Asset; Mesh Asset → Texture Asset when an explicit Texture Asset Reference exists in the mesh descriptor (`texture_guids`); each Asset → its Intermediate inputs (descriptor and Intermediate `source` file) as freshness leaves. `texture_guids` SHALL be the union of Import-discovered Texture Asset GUIDs and non-empty Mesh material override slot GUIDs. COLLADA `<image>` URIs that are not registered Texture Assets SHALL NOT appear as graph nodes. Material Asset nodes are out of v1.

#### Scenario: Texture Intermediate change invalidates dependent Mesh Final
- **WHEN** an Intermediate texture file changes for a Texture Asset referenced by a Mesh Asset via `texture_guids`
- **THEN** that Texture’s Final and the dependent Mesh’s Final are marked stale

#### Scenario: Scene depends on Mesh
- **WHEN** a Scene Asset references a Mesh Asset by GUID
- **THEN** the dependency graph contains a Scene→Mesh edge used for invalidation

#### Scenario: Override-only texture is a graph edge
- **WHEN** a Mesh material override slot references Texture Asset T that Import did not discover
- **THEN** the dependency graph contains a Mesh→T edge via `texture_guids`
