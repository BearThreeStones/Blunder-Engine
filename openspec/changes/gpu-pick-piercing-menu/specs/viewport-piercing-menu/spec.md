## ADDED Requirements

### Requirement: Piercing menu activation

When the user Ctrl+right-clicks inside the editor 3D viewport over pickable geometry, the engine SHALL open a selection piercing menu listing all pickable entities along the view ray through that pixel, ordered front-to-back.

#### Scenario: Open piercing menu

- **WHEN** the user Ctrl+right-clicks a viewport pixel that intersects one or more pickable meshes
- **THEN** the engine SHALL display a popup menu at the cursor listing each distinct hit entity by display name

#### Scenario: Single hit still shows menu

- **WHEN** the user Ctrl+right-clicks a pixel with only one pickable mesh hit
- **THEN** the piercing menu SHALL list that single entity

#### Scenario: No pickable hits

- **WHEN** the user Ctrl+right-clicks a viewport pixel with no pickable mesh along the ray
- **THEN** the piercing menu SHALL NOT open

#### Scenario: Click outside viewport

- **WHEN** the user Ctrl+right-clicks outside the editor viewport rect
- **THEN** the piercing menu SHALL NOT open

### Requirement: Piercing menu selection actions

Choosing an entry in the piercing menu SHALL update `EditorSelectionSystem` according to how the menu was opened.

#### Scenario: Replace selection from menu

- **WHEN** the user opens the piercing menu with Ctrl+right-click (without Shift) and selects an entity from the list
- **THEN** the engine SHALL call `EditorSelectionSystem::setSelection` with the chosen entity

#### Scenario: Add to selection from menu

- **WHEN** the user opens the piercing menu with Ctrl+Shift+right-click and selects an entity from the list
- **THEN** the engine SHALL call `EditorSelectionSystem::addToSelection` with the chosen entity

#### Scenario: Dismiss menu without change

- **WHEN** the user opens the piercing menu and dismisses it without choosing an entry
- **THEN** the editor selection SHALL remain unchanged

### Requirement: Multi-hit GPU depth peel

The piercing menu SHALL obtain its hit list using GPU entity-ID prepass depth peeling at the click pixel, returning distinct leaf entities in front-to-back order.

#### Scenario: Overlapping meshes listed in depth order

- **WHEN** multiple pickable mesh renderers overlap at the same viewport pixel along the view ray
- **THEN** the piercing menu hit list SHALL include each distinct entity ordered from nearest to farthest

#### Scenario: Alpha-cutout respected in peel passes

- **WHEN** a peeled pick pass encounters alpha-mask geometry below cutoff at the sampled pixel
- **THEN** that surface SHALL NOT produce a hit for that peel iteration

#### Scenario: Blend meshes skipped

- **WHEN** only blend-transparent meshes exist along the ray at the pixel
- **THEN** the piercing menu SHALL NOT open (same as no hit)

### Requirement: Same-pixel click cycling

Repeated left-clicks on the same viewport pixel SHALL cycle through the peeled hit list when more than one pickable entity exists at that pixel.

#### Scenario: Cycle forward on repeat click

- **WHEN** the user left-clicks the same viewport pixel twice without moving more than a small tolerance (default 3 px) and multiple entities are pickable at that pixel
- **THEN** the second click SHALL select the next entity in front-to-back order after the currently selected entity in that hit list (wrapping to front)

#### Scenario: Single hit unchanged

- **WHEN** the user repeats left-click on a pixel with only one pickable entity
- **THEN** behavior SHALL match a normal replace selection (no spurious cycling)

### Requirement: Piercing menu input priority

The piercing menu SHALL NOT open when a higher-priority interaction consumes Ctrl+right-click in the viewport.

#### Scenario: Gizmo active drag

- **WHEN** the transform gizmo is actively dragging
- **THEN** Ctrl+right-click SHALL NOT open the piercing menu

#### Scenario: Right-click camera orbit

- **WHEN** the user right-clicks without Ctrl for camera free-look
- **THEN** the piercing menu SHALL NOT open and camera interaction SHALL proceed as today

### Requirement: Piercing menu UI integration

The piercing menu SHALL be implemented as a Slint popup anchored to window coordinates, showing entity display names from the active `SceneInstance`.

#### Scenario: Display name in menu row

- **WHEN** the piercing menu lists an entity
- **THEN** each row SHALL show the entity's scene name (or a stable fallback such as `Entity #<id>`)

#### Scenario: Selection sync after menu pick

- **WHEN** the user selects an entity from the piercing menu
- **THEN** inspector and hierarchy panels SHALL refresh the same as a direct viewport pick
