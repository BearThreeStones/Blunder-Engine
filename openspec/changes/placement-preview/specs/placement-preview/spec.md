## Purpose

Transient viewport follow-mesh and Content Browser drag cursor while placing a Mesh Asset into the edited scene, without creating a document Entity until drop.

## ADDED Requirements

### Requirement: Placement Preview follows Ground placement
While a Content Browser drag of a Mesh Asset has the pointer over the editor viewport, the editor SHALL draw a Placement Preview of that Mesh Asset at Ground placement (Editor Camera ray through the pointer intersects world Z=0; a miss uses the world origin). The preview SHALL NOT be a scene Entity, SHALL NOT appear in the Outliner, and SHALL NOT push Document History. When the pointer leaves the viewport, or the drag source is not a Mesh Asset, the preview SHALL hide.

#### Scenario: Mesh drag over viewport shows follow-mesh
- **WHEN** the user drags a Mesh Asset from the Content Browser over the editor viewport
- **THEN** a Placement Preview of that mesh is visible at Ground placement under the pointer

#### Scenario: Preview hides over the Browser
- **WHEN** the user is dragging a Mesh Asset and the pointer is over the Content Browser
- **THEN** Placement Preview is not visible

#### Scenario: Scene drag has no Placement Preview
- **WHEN** the user drags a Scene Asset over the editor viewport
- **THEN** Placement Preview is not visible

### Requirement: Placement Preview matches spawned shading
Placement Preview SHALL use the same opaque Mesh Asset materials and scene lighting as the MeshRenderer that drop will spawn. It SHALL NOT use Mesh Preview studio lighting, ghost alpha, or wireframe as the product look.

#### Scenario: Preview uses scene materials
- **WHEN** Placement Preview is visible for a Mesh Asset that has materials
- **THEN** the preview is shaded with those materials in the editor viewport scene pass

### Requirement: Drop still spawns; preview is not the Entity
Releasing over the viewport on a Mesh Asset SHALL spawn as today (Ground placement, one Spawn Entity Command). The Placement Preview SHALL be cleared on drop, cancel, or drag end. The spawned Entity SHALL be a new document object, not a conversion of the preview.

#### Scenario: Drop seals spawn and clears preview
- **WHEN** the user releases a Mesh Asset Content Browser drag over the viewport
- **THEN** a scene Entity is spawned at Ground placement, Placement Preview is gone, and Document History has one Spawn Entity Command

### Requirement: Content Browser drag cursor is three-state
During an active Content Browser drag, the system cursor SHALL be: pointer when the pointer is over the editor viewport and the source is a Mesh Asset or a Scene Asset; move when over a Browser folder (reparent); not-allowed otherwise. The cursor SHALL restore when the drag ends or is cancelled. OS file drop onto the viewport SHALL NOT spawn or open in this slice.

#### Scenario: Mesh over viewport uses pointer
- **WHEN** a Mesh Asset is dragged over the editor viewport
- **THEN** the cursor is the pointer (hand) cursor

#### Scenario: Scene over viewport uses pointer
- **WHEN** a Scene Asset is dragged over the editor viewport
- **THEN** the cursor is the pointer cursor and Placement Preview stays hidden

#### Scenario: Folder hover uses move
- **WHEN** a Content Browser entry is dragged over a Browser folder
- **THEN** the cursor is the move cursor

#### Scenario: Invalid chrome uses not-allowed
- **WHEN** a Content Browser drag is over Inspector or other non-drop chrome
- **THEN** the cursor is the not-allowed cursor

### Requirement: Escape cancels Content Browser drag
Pressing Escape during a Content Browser drag SHALL abort the drag with no spawn, no scene open, and no folder reparent. Placement Preview and the drag cursor SHALL clear.

#### Scenario: Escape aborts in-viewport mesh drag
- **WHEN** the user is dragging a Mesh Asset over the viewport with Placement Preview visible and presses Escape
- **THEN** no entity is spawned, Placement Preview is gone, and the cursor is restored
