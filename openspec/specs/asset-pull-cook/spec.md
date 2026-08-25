# asset-pull-cook Specification

## Purpose
TBD - created by archiving change asset-pipeline-pull. Update Purpose after archive.
## Requirements
### Requirement: Pull pipeline freshness
The editor and runtime SHALL pull Assets by identity. Final Assets SHALL be produced by Cook when missing or stale relative to Intermediate inputs. A full-project Cook MAY run as warm-up or CI, but SHALL NOT be the only freshness mechanism for authoring.

#### Scenario: Load triggers on-demand Cook when Final missing
- **WHEN** an Asset is loaded and no fresh Final exists
- **THEN** the system uses Fast Path Intermediate data for the Loaded Asset and requests Cook for that Asset

#### Scenario: Fresh Final preferred
- **WHEN** an Asset is loaded and a Final exists that matches current Intermediate freshness
- **THEN** the Loaded Asset is built from the Final, not by re-parsing Intermediate as the primary path

### Requirement: Fast Path
When Final is missing or stale, the system SHALL load Intermediate data into a Loaded Asset without blocking the first preview on Cook completion. For mesh Assets, Intermediate SHALL be COLLADA (`.dae`) after successful Import or Intermediate Upgrade; legacy glTF/GLB Intermediate MAY be loaded only while `source` still points at that file. Fast Path and Final MAY coexist in one Editor Session.

#### Scenario: Stale cooked falls back to Intermediate COLLADA
- **WHEN** cooked meta indicates Intermediate or descriptor is newer than Final and Intermediate `source` is `.dae`
- **THEN** load uses Intermediate COLLADA (Fast Path) and treats Final as stale

#### Scenario: Legacy glTF Fast Path while upgrade pending
- **WHEN** Final is missing and Intermediate `source` still points at glTF/GLB (upgrade not yet successful)
- **THEN** load MAY use that legacy Intermediate for Fast Path

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

### Requirement: Asset Watch invalidation
The editor SHALL watch the Assets root and Resources root (Source archive and Intermediate bodies). Descriptor and other Assets-tree changes SHALL invalidate Finals for affected Assets and dependents via the dependency graph. Intermediate body changes that map to Assets SHALL be handled primarily by Detection Action → Reimport (see Detection Action specs); if Reimport does not run (Prompt dismissed or attribution miss), the editor SHALL still invalidate Finals for mapped Assets when a watched Intermediate or descriptor change is observed.

#### Scenario: Descriptor change invalidates Final
- **WHEN** a mesh or texture Asset Descriptor file changes on disk
- **THEN** that Asset’s Final is marked stale

#### Scenario: Intermediate change without Reimport still invalidates
- **WHEN** an Intermediate body mapped to an Asset changes and Detection Prompt is dismissed
- **THEN** that Asset’s Final is marked stale even though Reimport did not run

### Requirement: Source change triggers Reimport
Changes under the Source root for an archived Source file SHALL enter Detection Action for Assets that archive that Source (debounced, including sidecar attribution rules where applicable). Reimport SHALL run only when Detection Action is Auto or the user confirms Prompt — not as an unconditional silent auto-Reimport.

#### Scenario: Archived Source change with Auto Detection
- **WHEN** Detection Action is Auto and an archived Source file watched under the Source root changes
- **THEN** Reimport runs for the owning Asset GUID(s), refreshing Intermediate and invalidating Finals

#### Scenario: Archived Source change with Prompt Detection
- **WHEN** Detection Action is Prompt and an archived Source file changes
- **THEN** the editor prompts before Reimport and does not silently Reimport without confirmation

### Requirement: Intermediate Upgrade from legacy glTF
When a mesh Asset’s Intermediate `source` still points at glTF or GLB after the COLLADA Intermediate switch, the system SHALL perform a GUID-preserving Intermediate Upgrade on project open or registry scan: convert to `.dae`, rewrite descriptor `source`, archive the former glTF/GLB under the Source root when not already archived, and mark Finals stale. If conversion fails, the system SHALL leave the descriptor and glTF/GLB `source` unchanged, SHALL NOT point `source` at a partial `.dae`, and SHALL allow Fast Path/Cook to keep using that glTF/GLB until a later successful upgrade or Reimport.

#### Scenario: Successful lazy upgrade
- **WHEN** registry scan finds a mesh Asset whose Intermediate `source` is a glTF file that Assimp can convert
- **THEN** Intermediate `source` becomes a `.dae`, the former glTF is archived under Source when needed, the GUID is unchanged, and Finals are marked stale

#### Scenario: Failed upgrade leaves legacy source
- **WHEN** Intermediate Upgrade conversion fails for a mesh Asset
- **THEN** the descriptor and glTF/GLB `source` are unchanged and load MAY still use that legacy Intermediate

