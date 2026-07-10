## MODIFIED Requirements

### Requirement: Sync-then-async left-click selection

On the first unmodified left-click at a viewport pixel (not the local same-pixel cycle branch), the engine SHALL select the front-most pickable entity synchronously in the **input phase** before starting async `multi_peel`. The synchronous pick SHALL use the existing narrow `PickOverlay` path (`pickEntityAtWindowPosition` / `pickAllEntitiesAtWindowPosition`) with parent promotion. Async `multi_peel` SHALL use the hybrid broad-phase hit list to refine `m_last_peel_hits` without re-applying selection when the promoted front matches the sync result.

#### Scenario: Immediate selection on left-click release

- **WHEN** the user releases left-click on a viewport pixel with a pickable mesh hit (no Ctrl/Shift) and the local cycle branch does not apply
- **THEN** the engine SHALL perform a synchronous front-most pick, promote the hit, and call `applySelection` in the input phase before the next viewport render
- **AND** the engine SHALL populate `m_last_peel_hits` synchronously from `pickAllEntitiesAtWindowPosition` with deduplicated promoted ids
- **AND** the engine SHALL then start async `multi_peel` for broad-phase list refinement

#### Scenario: Outline and gizmo without camera move

- **WHEN** the user left-clicks a pickable mesh in the viewport and does not move the editor camera afterward
- **THEN** the selection outline and transform gizmo SHALL appear within 2 frames of the click release

#### Scenario: Async delivery updates cycle list only

- **WHEN** async `multi_peel` completes after a sync-then-async left-click and the promoted front hit matches the current primary selection
- **THEN** the engine SHALL update `m_last_peel_hits` for cycling and SHALL NOT change primary selection again

#### Scenario: Async fallback when sync misses

- **WHEN** synchronous front-most pick misses but async `multi_peel` returns at least one hit
- **THEN** the engine SHALL apply selection from the async promoted front hit

### Requirement: Same-pixel click cycling

Repeated left-clicks on the same viewport pixel SHALL cycle through the multi-hit pick list when more than one pickable leaf entity exists at that pixel. The list SHALL be populated synchronously on first click (from `pickAllEntitiesAtWindowPosition` with promotion) and MAY be refined by async broad-phase delivery. The peel list SHALL be ready for cycling on the second click without waiting for async completion.

#### Scenario: Cycle forward on repeat click

- **WHEN** the user left-clicks the same viewport pixel twice without moving more than a small tolerance (default 3 px) and multiple entities are pickable at that pixel
- **THEN** the second click SHALL select the next entity in front-to-back order after the currently selected entity in that hit list (wrapping to front)

#### Scenario: First click populates peel list

- **WHEN** the user left-clicks a viewport pixel (no Ctrl/Shift) where multiple pickable leaves overlap along the view ray
- **THEN** the engine SHALL populate `m_last_peel_hits` synchronously on release so cycling is available on the second click
- **AND** async `multi_peel` SHALL refine the list when it completes

#### Scenario: Single hit unchanged

- **WHEN** the user left-clicks a pixel with only one pickable entity along the ray
- **THEN** behavior SHALL match a normal replace selection (no spurious cycling on repeat click)
