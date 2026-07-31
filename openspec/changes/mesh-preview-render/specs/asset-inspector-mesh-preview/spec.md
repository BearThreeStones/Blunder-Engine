## ADDED Requirements

### Requirement: Browser Mesh selection enters Asset Inspector
When the user selects a Mesh Asset in the Content Browser, the Inspector SHALL enter Asset Inspector mode for that Asset. Asset Inspector mode SHALL show an interactive Mesh Preview fed by Mesh Preview Render and read-only identity fields (at least display name, GUID, type, and Intermediate source path when known). Asset Inspector mode SHALL NOT require editing Import settings in this slice.

#### Scenario: Select Sponza mesh descriptor
- **WHEN** the user selects `Sponza.mesh.yaml` (or equivalent Mesh Asset) in the Content Browser
- **THEN** the Inspector shows Mesh Preview for that Asset plus read-only identity, not only the Entity Transform UI for an unrelated selection

### Requirement: Mesh Preview interaction
Mesh Preview SHALL support left-button orbit, mouse-wheel zoom, and double-click reset to the default framed view. Orbit orientation SHALL be session-ephemeral and SHALL NOT be persisted as Asset or scene data in this slice. Pointer interaction over the preview SHALL NOT fall through to unrelated Inspector controls behind it.

#### Scenario: Orbit does not stick to disk
- **WHEN** the user orbits a Mesh Preview and later restarts the editor
- **THEN** Mesh Preview returns to the default framed view for that Asset

### Requirement: Entity selection restores Entity Inspector
When the user selects a scene Entity (viewport or Hierarchy), the Inspector SHALL leave Asset Inspector mode and show the Entity Inspector. Mesh Preview for a previous Browser Mesh selection SHALL not remain as the primary Inspector content after Entity selection.

#### Scenario: Pick entity after Mesh Asset
- **WHEN** Asset Inspector is showing a Mesh Preview and the user selects a scene Entity
- **THEN** the Inspector shows that Entity’s Inspector content instead of Asset Inspector mode

### Requirement: Non-Mesh Assets do not claim Mesh Preview
Selecting a non-Mesh Content Browser entry SHALL NOT open Mesh Preview. Texture and other types keep their existing Inspector/Browser behavior for this slice.

#### Scenario: Texture selection
- **WHEN** the user selects a Texture Asset in the Content Browser
- **THEN** the Inspector does not show Mesh Preview for that selection
