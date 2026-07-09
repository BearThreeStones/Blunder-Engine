## ADDED Requirements

### Requirement: Translate modal session from grab

The editor SHALL begin a Translate Modal Session when the user presses `G` with a valid selection and no translate modal session already active. The grab-started session SHALL use free (unconstrained) view-plane translation and SHALL record the selection's full transform at session start.

#### Scenario: G begins free translate session

- **WHEN** a scene entity is selected and no Translate Modal Session is active
- **AND** the user presses `G`
- **THEN** a Translate Modal Session begins with free view-plane translation
- **AND** the session records the selection's transform at session start

#### Scenario: G ignored without selection

- **WHEN** no entity is selected
- **AND** the user presses `G`
- **THEN** no Translate Modal Session begins

#### Scenario: G ignored while session already active

- **WHEN** a Translate Modal Session is already active
- **AND** the user presses `G`
- **THEN** the existing session continues
- **AND** a second session does not begin

### Requirement: Grab confirm and cancel

A Translate Modal Session started from grab SHALL confirm on LMB press (click-to-confirm), keeping the current entity transform. RMB or Escape SHALL cancel the session and restore the entity transform recorded at session start. Handle-started sessions SHALL keep release-to-confirm behavior.

#### Scenario: Grab LMB click confirms

- **WHEN** a grab-started Translate Modal Session is active
- **AND** the user presses LMB in the viewport
- **THEN** the session ends
- **AND** the entity remains at the current dragged transform

#### Scenario: Grab Escape cancels

- **WHEN** a grab-started Translate Modal Session is active
- **AND** the user presses Escape
- **THEN** the session ends
- **AND** the entity transform is restored to the session-start transform

#### Scenario: Grab RMB cancels

- **WHEN** a grab-started Translate Modal Session is active
- **AND** the user presses RMB
- **THEN** the session ends
- **AND** the entity transform is restored to the session-start transform

### Requirement: Grab session feedback

While a grab-started Translate Modal Session is active, the viewport SHALL draw no constraint guides and no active-handle drag ghost. The solid free-move center disc SHALL be hidden. The three reference axis arrows SHALL remain visible and follow the entity's current pivot. The four-way move cursor and transform-active outline SHALL apply for the session lifetime as for any active Translate Modal Session.

#### Scenario: Grab has no guides or ghost

- **WHEN** the user moves the pointer during a grab-started session
- **THEN** no constraint guide lines are drawn
- **AND** no translate-handle drag ghost is drawn

#### Scenario: Grab hides solid center

- **WHEN** a grab-started Translate Modal Session is active
- **THEN** the solid free-move center disc is hidden
- **AND** the three reference axis arrows remain visible at the entity's current pivot
