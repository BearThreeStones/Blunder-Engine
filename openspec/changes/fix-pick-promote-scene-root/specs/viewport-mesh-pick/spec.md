## ADDED Requirements

### Requirement: Scene-root-child pick promotion

When a viewport GPU pick resolves to a leaf mesh entity, the engine SHALL promote the hit to the **scene-root child** entity: walk up the hierarchy while the current entity's parent has a valid grandparent, then return the parent of the stopping entity (the entity whose own parent is invalid). If the leaf has no parent, the leaf SHALL be returned unchanged.

#### Scenario: glTF nested mesh promotes to scene root

- **WHEN** the user left-clicks a viewport pixel that hits a leaf mesh entity `L` where `L → N → R` and `R` has no parent (`R` is a scene-authored root such as `BoxFront`)
- **THEN** selection, piercing menu rows, and click-cycle entries SHALL use entity `R`, not `N` or `L`

#### Scenario: Root mesh entity unchanged

- **WHEN** the user left-clicks a pixel that hits a mesh renderer on an entity with no parent
- **THEN** the engine SHALL select that entity

#### Scenario: Two-level authored parent unchanged

- **WHEN** the user left-clicks a pixel that hits a child mesh whose parent has no grandparent (parent is a scene-root child)
- **THEN** the engine SHALL select the parent entity

### Requirement: Sync peel list on first left-click

On the first unmodified left-click at a viewport pixel (not the local same-pixel cycle branch), after synchronous front-most pick and `applySelection`, the engine SHALL populate `m_last_peel_hits` from a synchronous multi-hit entity peel at that pixel, with each hit walk-up promoted and deduplicated, before starting async `multi_peel`.

#### Scenario: Second click cycles without waiting for async peel

- **WHEN** the user left-clicks a pixel where ≥2 distinct scene-root-child entities overlap along the view ray and releases without modifiers
- **THEN** `m_last_peel_hits` SHALL contain at least two promoted entities before the next frame's async peel completes
- **AND** a second left-click at the same pixel (within tolerance) SHALL advance selection to the next entry in that list

#### Scenario: Single hit peel list

- **WHEN** only one pickable entity exists at the pixel after promotion and deduplication
- **THEN** `m_last_peel_hits` SHALL contain one entry and repeat clicks SHALL NOT spuriously cycle

## MODIFIED Requirements

### Requirement: Sync-then-async left-click selection

On the first unmodified left-click at a viewport pixel (not the local same-pixel cycle branch), the engine SHALL select the front-most pickable entity synchronously using **scene-root-child promotion** before starting async `multi_peel`.

#### Scenario: Immediate selection on left-click release

- **WHEN** the user releases left-click on a viewport pixel with a pickable mesh hit (no Ctrl/Shift) and the local cycle branch does not apply
- **THEN** the engine SHALL perform a synchronous front-most pick, promote the hit to the scene-root child, and call `applySelection` in the input phase before the next viewport render
- **AND** the engine SHALL populate `m_last_peel_hits` synchronously from multi-hit peel at that pixel
- **AND** the engine SHALL then start async `multi_peel` to refresh the peel hit list

#### Scenario: Async delivery updates cycle list only

- **WHEN** async `multi_peel` completes after a sync-then-async left-click and the promoted front hit matches the current primary selection
- **THEN** the engine SHALL update `m_last_peel_hits` for cycling and SHALL NOT change primary selection again

#### Scenario: Async fallback when sync misses

- **WHEN** synchronous front-most pick misses but async `multi_peel` returns at least one hit
- **THEN** the engine SHALL apply selection from the async promoted front hit using scene-root-child promotion

### Requirement: Same-pixel click cycling

Repeated left-clicks on the same viewport pixel SHALL cycle through the multi-hit pick list when more than one **scene-root-child promoted** entity exists at that pixel. The list SHALL be produced by entity-ID peel (exclude prior hits from draw set each iteration), not depth peel. The peel list SHALL be populated synchronously on first click and refreshed by async `multi_peel` after the first unmodified left-click at a pixel.

#### Scenario: Cycle forward on repeat click

- **WHEN** the user left-clicks the same viewport pixel twice without moving more than a small tolerance (default 3 px) and multiple promoted scene-root-child entities are pickable at that pixel
- **THEN** the second click SHALL select the next entity in front-to-back order after the currently selected entity in that hit list (wrapping to front)

#### Scenario: First click populates peel list

- **WHEN** the user left-clicks a viewport pixel (no Ctrl/Shift) where multiple pickable leaves overlap along the view ray
- **THEN** the engine SHALL populate `m_last_peel_hits` synchronously with all distinct promoted scene-root-child hits (up to `k_max_peel_count`)
- **AND** the engine SHALL start async `multi_peel` to refresh the list

#### Scenario: Single hit unchanged

- **WHEN** the user left-clicks a pixel with only one pickable entity along the ray after promotion
- **THEN** `multi_peel` SHALL terminate after one hit and behavior SHALL match a normal replace selection (no spurious cycling on repeat click)

### Requirement: Piercing menu seeds click cycle list

When the user opens the piercing menu at a pixel, subsequent left-clicks at the same pixel (within tolerance, no modifiers) SHALL cycle through the same **scene-root-child promoted** hit list without requiring a new GPU peel if the list is already cached.

#### Scenario: Left-click after piercing menu

- **WHEN** the user opens the piercing menu with ≥2 rows at pixel P and then left-clicks at pixel P without modifiers
- **THEN** selection SHALL advance to the next entry in the cached peel list (or the first if none was selected from that list)
