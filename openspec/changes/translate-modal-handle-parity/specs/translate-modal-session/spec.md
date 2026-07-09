## ADDED Requirements

### Requirement: Translate modal session from handle drag

The editor SHALL run translate handle interaction through a Translate Modal Session. Starting a drag on a translate axis, plane, or center handle SHALL begin a session that owns motion mapping, constraint guides, drag ghost, origin-dot state, handle visibility, cursor, and confirm/cancel until the session ends.

#### Scenario: Axis handle begins session

- **WHEN** translate mode is active and the user presses LMB on a visible translate axis arrow
- **THEN** a Translate Modal Session begins with a single-axis constraint for that axis
- **AND** the session records the selection's transform at drag start

#### Scenario: Plane handle begins session

- **WHEN** translate mode is active and the user presses LMB on a translate plane handle
- **THEN** a Translate Modal Session begins with a two-axis plane constraint for that plane
- **AND** the plane constraint uses an infinite constraint plane

#### Scenario: Center handle begins session

- **WHEN** translate mode is active and the user presses LMB on the translate center handle
- **THEN** a Translate Modal Session begins with free (unconstrained) view-plane translation

### Requirement: Screen-space translate motion

While a Translate Modal Session is active, entity translation SHALL be computed from screen-space mouse motion projected into the view, then constrained according to the session constraint. Finite plane-quad tracking MUST NOT be used for plane motion.

#### Scenario: Axis drag follows screen motion along axis

- **WHEN** the user drags a translate axis handle under a static camera
- **THEN** the selected entity moves along that axis in response to mouse motion
- **AND** motion remains continuous when the pointer leaves the on-screen arrow geometry

#### Scenario: Plane drag uses infinite plane

- **WHEN** the user drags a translate plane handle
- **THEN** the selected entity moves in that plane for the duration of the drag
- **AND** motion continues when the pointer leaves the plane handle's on-screen square

#### Scenario: Center drag is free view-plane motion

- **WHEN** the user drags the translate center handle
- **THEN** the selected entity moves in the camera view plane according to mouse delta
- **AND** no constraint guide lines are drawn

### Requirement: Constraint guides during translate session

While a constrained Translate Modal Session is active, the viewport SHALL draw constraint guides through the drag-start pivot: one axis-colored line for a single-axis constraint, two axis-colored lines for a plane constraint. Guides SHALL remain pinned at the drag-start pivot while the object moves. Free center sessions SHALL draw no constraint guides.

#### Scenario: Single-axis guide pinned at drag start

- **WHEN** the user drags the X translate arrow and the entity moves
- **THEN** one X-colored constraint guide remains through the drag-start pivot
- **AND** the guide does not translate with the entity

#### Scenario: Plane guides are two axes

- **WHEN** the user drags the XY plane handle
- **THEN** X- and Y-colored constraint guides are drawn through the drag-start pivot

### Requirement: Drag ghost of active handle

While a Translate Modal Session is active, the viewport SHALL draw a semi-transparent drag ghost of only the active translate handle at the drag-start pose.

#### Scenario: Axis drag ghost

- **WHEN** the user drags a translate axis arrow
- **THEN** a semi-transparent copy of that axis handle remains at the drag-start pose
- **AND** non-active handles are not ghosted as a full gizmo group

#### Scenario: Center drag ghost

- **WHEN** the user drags the translate center handle
- **THEN** a semi-transparent center disc ghost remains at the drag-start pose
- **AND** the solid center disc is not shown at the live pivot during the session

### Requirement: Handle visibility during translate session

During an active Translate Modal Session the viewport SHALL keep the three reference axis arrows visible and following the entity's current pivot. Plane-handle visibility SHALL depend on the active handle: axis drag hides all plane handles; plane drag keeps only the active plane handle visible and hides the other plane handles. The solid free-move center disc SHALL be hidden for the session duration.

#### Scenario: Axis drag hides planes

- **WHEN** the user drags a translate axis arrow
- **THEN** all plane handles are hidden
- **AND** the three axis arrows remain visible at the entity's current pivot

#### Scenario: Plane drag keeps active plane

- **WHEN** the user drags the YZ plane handle
- **THEN** the YZ plane handle remains visible
- **AND** the XY and ZX plane handles are hidden
- **AND** the three axis arrows remain visible at the entity's current pivot

### Requirement: Origin dot during constrained translate

While a constrained Translate Modal Session is active, the viewport SHALL draw an origin dot: for a single-axis constraint, at the root of the active axis arrow in that axis color; for a plane constraint, on the visible active plane handle in the plane's normal-axis color (XY→Z, YZ→X, ZX→Y). Free center sessions SHALL NOT require a live origin dot on a solid center disc.

#### Scenario: Axis origin dot color

- **WHEN** the user drags the Y translate arrow
- **THEN** an origin dot is shown at the Y arrow root in the Y axis color

#### Scenario: Plane origin dot uses normal axis color

- **WHEN** the user drags the XY plane handle
- **THEN** an origin dot is shown on the XY plane handle in the Z axis color

### Requirement: Cursor during translate session

While a Translate Modal Session is active, the editor SHALL show a four-way move cursor. The cursor SHALL restore when the session ends. Hovering a translate handle without an active session MUST NOT change the cursor for this requirement.

#### Scenario: Cursor while dragging

- **WHEN** a translate handle drag session is active
- **THEN** the cursor is the four-way move style

#### Scenario: Cursor restored on end

- **WHEN** the session ends by confirm or cancel
- **THEN** the cursor is restored to the pre-session cursor

### Requirement: Handle confirm and cancel

A Translate Modal Session started from a handle SHALL confirm on LMB release, keeping the current entity transform. RMB or Escape SHALL cancel the session and restore the entity transform recorded at session start.

#### Scenario: Release confirms

- **WHEN** the user releases LMB after dragging a translate handle
- **THEN** the session ends
- **AND** the entity remains at the dragged transform

#### Scenario: Escape cancels

- **WHEN** the user presses Escape during a translate handle drag
- **THEN** the session ends
- **AND** the entity transform is restored to the drag-start transform

#### Scenario: Right mouse cancels

- **WHEN** the user presses RMB during a translate handle drag
- **THEN** the session ends
- **AND** the entity transform is restored to the drag-start transform
