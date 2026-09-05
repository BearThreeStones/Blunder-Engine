# editor-camera-fly Specification

## Purpose

Editor Camera WASD / Q / E fly only while the author is already using viewport right- or middle-mouse camera control, so those keys do not move the view from Hierarchy or Inspector.

## Requirements

### Requirement: Fly requires viewport-started mouse hold
Editor Camera fly (W, A, S, D, Q, E, with Shift as sprint) SHALL run only while right mouse or middle mouse was pressed with the pointer in the editor viewport and that button is still held. Middle-mouse pan SHALL still run during that hold. Pointer over the viewport without that button SHALL NOT fly. Pointer over Hierarchy, Inspector, or Content Browser SHALL NOT fly. After mouse capture begins, fly SHALL continue if the pointer leaves the viewport rectangle until that button is released. Gameplay Input WASD in the Player SHALL be unchanged.

#### Scenario: Hover over viewport does not fly
- **WHEN** the pointer is over the editor viewport
- **AND** neither right nor middle mouse is held
- **AND** W is held
- **THEN** the Editor Camera position does not change from fly

#### Scenario: Right-mouse hold flies
- **WHEN** right mouse was pressed in the editor viewport and is still held
- **AND** W is held
- **THEN** the Editor Camera moves forward (fly)

#### Scenario: Middle-mouse hold flies and pans
- **WHEN** middle mouse was pressed in the editor viewport and is still held
- **AND** W is held
- **THEN** the Editor Camera moves from fly
- **AND** pointer motion still pans

#### Scenario: Hierarchy WASD does not fly
- **WHEN** the pointer is over the Hierarchy Panel
- **AND** W is held
- **THEN** the Editor Camera position does not change from fly

#### Scenario: Capture may leave the viewport
- **WHEN** right mouse was pressed in the editor viewport and is still held
- **AND** mouse capture has moved the pointer outside the viewport rectangle
- **AND** D is held
- **THEN** the Editor Camera still flies
