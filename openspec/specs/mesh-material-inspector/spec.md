# mesh-material-inspector Specification

## Purpose
Authors edit a Mesh Asset’s Import-built MaterialAsset from Asset Inspector through a sparse descriptor override, with Inspector property-field chrome and Global History, without a Material Asset type.

## Requirements

### Requirement: Material Inspector on Mesh Asset Inspector
When the Inspector selection is a Mesh Asset, Asset Inspector SHALL show a Material Inspector section in addition to Mesh Preview and read-only identity. Material Inspector SHALL edit Unlit, Base Color, Metallic, Roughness, Ambient, Diffuse, Specular, Shininess, and texture slots Base Color, Metallic-Roughness, Normal, and Occlusion. Material Inspector SHALL NOT appear on Entity Inspector. This slice SHALL NOT introduce a Content Browser Material Asset type.

#### Scenario: Mesh Asset selection shows Material Inspector
- **WHEN** the author selects a Mesh Asset in the Content Browser
- **THEN** Inspector shows Mesh Preview, identity, and Material Inspector

#### Scenario: Entity selection hides Material Inspector
- **WHEN** the author selects a scene entity
- **THEN** Inspector does not show Material Inspector

### Requirement: Entity Inspector does not host Editor shading overrides
Entity Inspector SHALL NOT show process-global Blinn-Phong light direction/color, ambient/diffuse/specular/shininess/unlit, or SSAO knobs, including Sync Asset and Reset for that block.

#### Scenario: MeshRenderer entity has no shading sliders
- **WHEN** the author selects a scene entity with a MeshRenderer
- **THEN** Inspector does not show Dir X/Y/Z, ka/kd/ks sliders, or SSAO Enabled from Editor shading overrides

### Requirement: Inspector property fields and object cells
Material Inspector scalar, color, and toggle rows SHALL use Inspector property fields (Godot compact 22px cells), not Editor Slider, Toggle, or Color Field chrome. Texture slots SHALL use Inspector object cells: label, recessed cell showing the Texture Asset display name or None, pick, clear, and Content Browser drop of a Texture Asset. Behavior MAY match Editor Object Field; the skin SHALL NOT be Editor Object Field.

#### Scenario: Texture slot is an Inspector object cell
- **WHEN** Material Inspector is visible
- **THEN** each texture slot is a labeled Inspector object cell, not an Editor Object Field

#### Scenario: Drop Texture Asset onto a slot
- **WHEN** the author drops a Texture Asset from the Content Browser onto a Material Inspector texture slot
- **THEN** that slot’s override key stores that Texture Asset GUID

### Requirement: Sparse Mesh material override
Material Inspector commits SHALL persist as a sparse bag of authored keys on that Mesh Asset descriptor. Load SHALL build the Import MaterialAsset for the Mesh Asset’s one surface, then overlay stored keys. An absent key SHALL keep the Import value. A slot key with a Texture Asset GUID SHALL use that Asset. A slot key that is empty SHALL suppress the Import texture on that slot and SHALL NOT delete the Texture Asset. An untouched slot SHALL NOT be stored as an empty override. The bag SHALL overlay only the Mesh Asset’s one MaterialAsset (the surface `loadMesh` / `getMaterialAsset()` use; for glTF, the first primitive). Extra primitives SHALL keep Import-built materials. Inspector edits SHALL NOT write glTF or Source files.

#### Scenario: Unedited field follows Import after Reimport
- **WHEN** the author has overridden Shininess only
- **AND** Reimport rebuilds the Import MaterialAsset with a new Base Color
- **THEN** Shininess stays the override value and Base Color matches the new Import value

#### Scenario: Empty slot suppresses Import texture
- **WHEN** the author clears the Base Color texture slot
- **THEN** that slot has an empty override key and the Import Base Color texture is not sampled
- **AND** the Texture Asset still exists

#### Scenario: Extra glTF primitives stay Import
- **WHEN** a Mesh Asset’s Intermediate has multiple primitives with distinct Import materials
- **AND** the author edits Material Inspector Base Color
- **THEN** only the first primitive’s MaterialAsset shows the override
- **AND** other primitives keep their Import-built materials

### Requirement: texture_guids unions non-empty override slots
The Mesh descriptor `texture_guids` list SHALL remain the Mesh→Texture graph edge list and SHALL be the union of Import-discovered Texture Asset GUIDs and non-empty Mesh material override slot GUIDs. Clearing a slot to empty SHALL NOT remove Import-discovered GUIDs from `texture_guids`. Replacing `texture_guids` wholesale with only the four slots SHALL NOT occur.

#### Scenario: Override slot GUID is a graph edge
- **WHEN** the author assigns Texture Asset T to the Normal slot and T was not Import-discovered
- **THEN** T’s GUID is present in `texture_guids`

#### Scenario: Empty slot keeps Import GUID in the list
- **WHEN** Import discovered Base Color texture U
- **AND** the author clears the Base Color slot
- **THEN** U’s GUID remains in `texture_guids`

### Requirement: Field commits are Global Commands
Each committed Material Inspector field (focus loss, Enter, or slot pick, clear, or drop) SHALL seal one Global Command. Dragging a value SHALL NOT push a Command per pointer move. Those Commands SHALL NOT be recorded on Document History.

#### Scenario: Enter commits one Global Command
- **WHEN** Asset Inspector has focus and the author changes Shininess and presses Enter
- **THEN** Global History can undo that Shininess change
- **AND** Document History is unchanged

### Requirement: Reset overrides
Material Inspector SHALL provide Reset overrides as Inspector chrome (same class as Add… / Remove: an Editor control, not a property-field row). Activating it SHALL seal one Global Command that deletes the whole sparse override bag so Import MaterialAsset values show through. Reset overrides SHALL NOT Reimport and SHALL NOT delete Texture Assets. This slice SHALL NOT provide per-field Revert.

#### Scenario: Reset restores Import without Reimport
- **WHEN** the Mesh descriptor has sparse override keys
- **AND** the author activates Reset overrides
- **THEN** the override bag is empty
- **AND** displayed and shaded values match the current Import-built MaterialAsset
- **AND** Intermediate `source` is unchanged
