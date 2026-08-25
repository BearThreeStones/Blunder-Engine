## Purpose

Editor Shell layout and chrome: Application Bar, dock frames, restyled Viewport tool-strip overlays, and Editor modal windows — without moving tools or rewriting panel interiors.

## ADDED Requirements

### Requirement: Application Bar layout
The Editor Session window SHALL have an Application Bar on Base 1, about 48px tall, darker than Window. Save / Undo (and Redo / Save As) SHALL sit on the left as ghost buttons (fill on hover only). Play / Pause / Stop SHALL be a centered segmented cluster whose Play control uses the **Editor accent**. View SHALL sit on the right. The bar SHALL NOT introduce native File/Edit menus in this pass and SHALL NOT host Move / Rotate / Scale.

#### Scenario: Play is centered
- **WHEN** an Editor Session is shown
- **THEN** Play / Pause / Stop form a cluster at the horizontal center of the Application Bar
- **AND** Save and Undo are on the left of that cluster
- **AND** View is on the right

#### Scenario: Save is ghost until hover
- **WHEN** the pointer is not over Save
- **THEN** Save has no filled button background
- **AND** hovering Save fills the ghost control

### Requirement: Viewport tool strips stay overlays
Transform tools, projection toggle, and animation preview SHALL remain Slint overlays on the editor viewport. They SHALL use Editor controls and a floating Toolbar treatment (translucent Base 3, hairline, 10px radius, accent on the checked tool). They SHALL NOT move onto the Application Bar or become a Scene window Toolbar in this pass.

#### Scenario: Transform tools overlay the viewport
- **WHEN** the Scene viewport is visible
- **THEN** Move / Rotate / Scale appear as an overlay on the viewport, not as Application Bar buttons

#### Scenario: Checked tool uses accent
- **WHEN** Move is the active transform tool
- **THEN** the Move control is visually checked with the Editor accent

### Requirement: Editor modal chrome
Authored Slint dialogs (Import Mesh, dirty Play, dirty Open, Detection reimport, Browser Delete, Project Manager Create/Import) SHALL use Editor modal chrome: 14px radius, no titlebar divider, dim overlay, actions bottom-right, confirming action as the single accent primary. Copy, button sets, and dialog behavior SHALL stay as authored. These flows SHALL NOT switch to native OS dialogs in this pass.

#### Scenario: Dirty Play modal primary is Save
- **WHEN** the dirty-Play dialog is shown
- **THEN** Save is the accent primary on the bottom-right
- **AND** Don't Save and Cancel are non-primary Editor controls Buttons
- **AND** the dialog copy is unchanged

### Requirement: Docked window frames follow Editor Theme
Docked and floating panel frames SHALL use Window fill, hairline, and 10px corner radius. Panel interiors keep their own specs. Splitter hit areas SHALL remain usable.

#### Scenario: Docked Hierarchy sits on Window
- **WHEN** Hierarchy is docked in the Editor Session
- **THEN** its frame fill is Editor Theme Window, not a leftover hardcoded non-Window island
