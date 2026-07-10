## MODIFIED Requirements

### Requirement: Same-pixel click cycling

Repeated left-clicks on the same viewport pixel SHALL cycle through the multi-hit pick list when more than one pickable leaf entity exists at that pixel. The list SHALL be produced by **entity-ID peel** (exclude prior hits from draw set each iteration), not depth peel. The peel list SHALL be populated by async `multi_peel` after the first unmodified left-click at a pixel.

#### Scenario: Cycle forward on repeat click

- **WHEN** the user left-clicks the same viewport pixel twice without moving more than a small tolerance (default 3 px) and multiple entities are pickable at that pixel
- **THEN** the second click SHALL select the next entity in front-to-back order after the currently selected entity in that hit list (wrapping to front)

#### Scenario: First click populates peel list

- **WHEN** the user left-clicks a viewport pixel (no Ctrl/Shift) where multiple pickable leaves overlap along the view ray
- **THEN** the engine SHALL start async `multi_peel` and the delivered result SHALL include all distinct front-to-back hits (up to `k_max_peel_count`) so `m_last_peel_hits` is ready for cycling on the second click

#### Scenario: Single hit unchanged

- **WHEN** the user left-clicks a pixel with only one pickable entity along the ray
- **THEN** `multi_peel` SHALL terminate after one hit and behavior SHALL match a normal replace selection (no spurious cycling on repeat click)

### Requirement: Async pick delivery on idle viewport

The engine SHALL poll and deliver async hybrid GPU pick results every frame while a pick is in flight, including when the viewport render path skips full scene rendering because the camera is unchanged.

#### Scenario: Pick completes with static camera

- **WHEN** the user left-clicks the viewport and the editor camera does not move afterward
- **THEN** `pollHybridPick` SHALL run on subsequent engine frames until the pick result is delivered

#### Scenario: Pick delivery before overlay sync

- **WHEN** an async hybrid pick result becomes ready during `tickVulkan`
- **THEN** `pollHybridPick` SHALL run before `OverlaySystem::begin_sync` for that frame

#### Scenario: Pick request schedules redraw

- **WHEN** `ViewportPickSystem::requestPick` submits an async pick
- **THEN** the render system SHALL mark the viewport dirty so in-flight peel passes can progress

### Requirement: Piercing menu seeds click cycle list

When the user opens the piercing menu at a pixel, subsequent left-clicks at the same pixel (within tolerance, no modifiers) SHALL cycle through the same promoted hit list without requiring a new GPU peel if the list is already cached.

#### Scenario: Left-click after piercing menu

- **WHEN** the user opens the piercing menu with ≥2 rows at pixel P and then left-clicks at pixel P without modifiers
- **THEN** selection SHALL advance to the next entry in the cached peel list (or the first if none was selected from that list)

## ADDED Requirements

### Requirement: Sync-then-async left-click selection

On the first unmodified left-click at a viewport pixel (not the local same-pixel cycle branch), the engine SHALL select the front-most pickable entity synchronously before starting async `multi_peel`.

#### Scenario: Immediate selection on left-click release

- **WHEN** the user releases left-click on a viewport pixel with a pickable mesh hit (no Ctrl/Shift) and the local cycle branch does not apply
- **THEN** the engine SHALL perform a synchronous front-most pick, promote the hit, and call `applySelection` in the input phase before the next viewport render
- **AND** the engine SHALL then start async `multi_peel` for the peel hit list

#### Scenario: Async delivery updates cycle list only

- **WHEN** async `multi_peel` completes after a sync-then-async left-click and the promoted front hit matches the current primary selection
- **THEN** the engine SHALL update `m_last_peel_hits` for cycling and SHALL NOT change primary selection again

#### Scenario: Async fallback when sync misses

- **WHEN** synchronous front-most pick misses but async `multi_peel` returns at least one hit
- **THEN** the engine SHALL apply selection from the async promoted front hit

### Requirement: Piercing menu blocks viewport pick gestures

While the piercing menu is visible, viewport pick and camera mouse routing SHALL NOT process the same pointer gestures intended for the menu.

#### Scenario: Menu open blocks viewport routing

- **WHEN** the piercing menu is visible and the user presses or releases a mouse button over the viewport
- **THEN** `shouldRouteMouseToInputLayers` SHALL return false and viewport pick/camera handlers SHALL NOT run for that event

#### Scenario: Menu row selection not overwritten by release

- **WHEN** the user selects an entity from the piercing menu with a left click
- **THEN** the matching left-button release SHALL NOT trigger viewport left-release pick or same-pixel cycle for that gesture
- **AND** the primary selection SHALL remain the menu-chosen entity
