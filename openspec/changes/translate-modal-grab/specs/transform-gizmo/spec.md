## ADDED Requirements

### Requirement: Grab key starts translate modal

When a selection exists and no Translate Modal Session is active, pressing `G` SHALL start a grab-entry Translate Modal Session for free view-plane translation. The key SHALL be handled by the transform gizmo / editor 3D shortcut path and MUST NOT require the translate gizmo mode to already be selected. While a grab session is active, the first LMB press in the viewport SHALL confirm the session before viewport mesh picking or gizmo handle picking runs.

#### Scenario: G starts grab without translate mode

- **WHEN** an entity is selected and the transform gizmo mode is not translate
- **AND** the user presses `G`
- **THEN** a grab-started Translate Modal Session begins
- **AND** the selected entity follows free view-plane mouse motion

#### Scenario: Grab LMB consumed before pick

- **WHEN** a grab-started Translate Modal Session is active
- **AND** the user presses LMB over a mesh or gizmo handle
- **THEN** the session confirms
- **AND** the press does not start a new gizmo handle drag or change selection via pick
