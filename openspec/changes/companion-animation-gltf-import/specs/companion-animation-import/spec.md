## ADDED Requirements

### Requirement: Companion Animation glTF acceptance
A glTF/GLB SHALL be accepted as a Companion Animation glTF when it contains one or more animations and has no meshes (meshes empty or count zero). Skins MAY be present. A glTF that fails acceptance SHALL NOT be attached as a companion to a Mesh Import.

#### Scenario: Chocomel LOOP shape accepted
- **WHEN** Import evaluates a glTF with animations=1, meshes=0, and skins=1
- **THEN** the file is accepted as a Companion Animation glTF

#### Scenario: Skinned character mesh rejected as companion
- **WHEN** Import evaluates a glTF that has skins and one or more meshes
- **THEN** the file is not attached as a companion (it may still Import as its own Mesh Asset)

### Requirement: Multi-select host and companion pairing
When Import receives a multi-select batch of glTF/GLB paths with animations enabled, Import SHALL treat exactly one skinned mesh candidate as the host Mesh. Other paths that pass Companion acceptance SHALL attach to that host. If the batch contains multiple skinned mesh candidates, each SHALL Import as its own Mesh; companion candidates without a single unambiguous host SHALL be skipped with a warning. Soft textures and non-glTF files in the batch SHALL continue existing Import handling.

#### Scenario: Chocomel multi-select
- **WHEN** the user multi-selects `Chocomel.gltf` plus `LOOP-chocomel-idle.gltf` and `LOOP-chocomel-walk.gltf` for Import with animations enabled
- **THEN** one Mesh Asset is registered for Chocomel and AnimationClip Assets are registered for the idle and walk companions under that mesh stem

#### Scenario: Orphan companions warned
- **WHEN** a batch contains only Companion-accepted glTFs and no skinned mesh host
- **THEN** Import does not silently invent a Mesh host and logs a warning for the orphan companions

### Requirement: Near-disk companion discovery is secondary
When Importing a single mesh glTF with animations enabled, Import MAY also discover Companion candidates in the mesh file’s directory and in immediate child directories of its parent directory, apply the same acceptance rules, and attach accepted companions. Near-disk discovery SHALL NOT recursively walk the whole Project or hard-code a DogWalk `animations/world` tree. Discovery SHALL run even when the mesh glTF already embeds animations.

#### Scenario: Co-located companion folder
- **WHEN** a mesh glTF is Imported and a sibling child folder under its parent contains an accepted companion glTF
- **THEN** clips from that companion are registered under the mesh stem in addition to any embedded animations

#### Scenario: Disconnected Chocomel trees need multi-select
- **WHEN** companions live only under a disconnected `animations/world/…` tree relative to `assets/char/chocomel/…`
- **THEN** near-disk discovery alone does not attach those companions; multi-select Import is required

### Requirement: Companion Intermediate copy without Mesh Asset
Accepted companions SHALL be copied under the Resources root as Intermediate bodies associated with the host mesh for later re-extract. Import SHALL NOT register companion files as Mesh Assets. Durable clip identity SHALL remain AnimationClip Assets whose Intermediate bodies are readable YAML.

#### Scenario: Companion not a Mesh Asset
- **WHEN** a companion is attached during mesh Import
- **THEN** a Mesh Asset GUID is not allocated for the companion file, and AnimationClip Assets exist for extracted animations

### Requirement: Companion clip naming and bone mismatch
Logical AnimationClip names from companions SHALL prefer the companion file stem; multi-animation companions SHALL disambiguate with a stable suffix. If companion track bone names share little or no overlap with the host mesh Skeleton, Import SHALL still register the clip and SHALL log a warning; Import SHALL NOT fail the host Mesh solely for that mismatch.

#### Scenario: Stem name for LOOP walk
- **WHEN** Import extracts clips from `LOOP-chocomel-walk.gltf`
- **THEN** the primary logical clip name is based on `LOOP-chocomel-walk` (sanitized as needed for the map)

#### Scenario: Bone mismatch still registers
- **WHEN** a companion’s animation channels do not match host skeleton bone names
- **THEN** the AnimationClip Asset is still registered and a warning is logged

### Requirement: Reimport refreshes companion-derived clips
Reimport of the host Mesh SHALL re-extract AnimationClip YAML from both the mesh Intermediate and stored companion Intermediate bodies, preserving clip Asset GUIDs when clip identity is stable.

#### Scenario: Reimport keeps companion clip GUIDs
- **WHEN** Reimport runs for a Mesh that previously attached companions
- **THEN** companion-derived AnimationClip YAML is refreshed and stable clip identities keep their GUIDs
