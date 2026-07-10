## MODIFIED Requirements

### Requirement: Same-pixel click cycling

Repeated left-clicks on the same viewport pixel SHALL cycle through the multi-hit pick list when more than one **scene-root-child promoted** entity exists at that pixel. The list SHALL be produced by entity-ID peel (exclude prior leaf hits from draw set each iteration), not depth peel. The peel list SHALL be available for the **second** click without waiting for async `multi_peel` to complete when sync peel on the first click populated `m_last_peel_hits`.

#### Scenario: Cycle forward on repeat click

- **WHEN** the user left-clicks the same viewport pixel twice without moving more than a small tolerance (default 3 px) and multiple promoted scene-root-child entities are pickable at that pixel
- **THEN** the second click SHALL select the next entity in front-to-back order after the currently selected entity in that hit list (wrapping to front)
- **AND** Hierarchy SHALL update on the second click in the input phase (sync cycle branch)

#### Scenario: First click populates peel list synchronously

- **WHEN** the user left-clicks a viewport pixel (no Ctrl/Shift) where multiple pickable leaves overlap along the view ray
- **THEN** the engine SHALL synchronously fill `m_last_peel_hits` on release before starting async `multi_peel`
- **AND** when async `multi_peel` completes, the engine MAY refine `m_last_peel_hits` without changing primary selection if the promoted front hit matches the current primary

#### Scenario: Single hit unchanged

- **WHEN** the user left-clicks a pixel with only one pickable entity along the ray after promotion
- **THEN** sync and async peel SHALL terminate with one promoted hit and behavior SHALL match a normal replace selection (no spurious cycling on repeat click)

## ADDED Requirements

### Requirement: Sync-then-async left-click selection

On the first unmodified left-click at a viewport pixel (not the local same-pixel cycle branch), the engine SHALL select the front-most pickable entity synchronously in the **input phase** before starting async `multi_peel`.

#### Scenario: Immediate selection on left-click release

- **WHEN** the user releases left-click on a viewport pixel with a pickable mesh hit (no Ctrl/Shift) and the local cycle branch does not apply
- **THEN** the engine SHALL perform a synchronous front-most pick, promote the hit, and call `applySelection` in the input phase before the next viewport render
- **AND** the engine SHALL synchronously populate `m_last_peel_hits` for same-pixel cycling
- **AND** the engine SHALL then start async `multi_peel` for peel-list refinement

#### Scenario: Async delivery updates cycle list only

- **WHEN** async `multi_peel` completes after a sync-then-async left-click and the promoted front hit matches the current primary selection
- **THEN** the engine SHALL update `m_last_peel_hits` for cycling and SHALL NOT change primary selection again

#### Scenario: Async fallback when sync misses

- **WHEN** synchronous front-most pick misses but async `multi_peel` returns at least one hit
- **THEN** the engine SHALL apply selection from the async promoted front hit

#### Scenario: Viewport left-click on pick_test

- **WHEN** the user left-clicks a mesh on `BoxFront` in `pick_test.scene.asset` at the overlap pixel (±Y view)
- **THEN** Hierarchy SHALL highlight `BoxFront` and the viewport SHALL show gizmo and selection outline within ≤2 frames without moving the camera

### Requirement: Left-release pick without prior press

When left-button release occurs inside the viewport without a matching `onViewportLeftPressed` in the same gesture, the engine SHALL still perform viewport pick if the release position is inside the viewport rect and drag/orbit guards pass.

#### Scenario: Release-only click in viewport

- **WHEN** left-button release occurs at a viewport pixel that was not preceded by `onViewportLeftPressed` for that gesture, and the pointer did not exceed the click-drag threshold
- **THEN** the engine SHALL run the same sync-then-async left-click pick path as a normal click

## REMOVED Requirements

### Requirement: Async-first left-click selection

**Reason:** Implemented and QA-failed on `pick_test`: Hierarchy updates from `onAsyncPickFirstPass` inside `tickVulkan`, but outline/gizmo require camera movement; same-pixel cycle does not run because `m_last_peel_hits` is not filled before the second click.

**Migration:** Restore sync-then-async on release; remove `applySelection` from async first peel pass.

### Requirement: Sync-then-async left-click selection (removed in prior delta)

**Reason:** Incorrectly removed in the async-first pivot. Restored above with sync `m_last_peel_hits` fill on release.

**Migration:** N/A — requirement restored in this delta.
