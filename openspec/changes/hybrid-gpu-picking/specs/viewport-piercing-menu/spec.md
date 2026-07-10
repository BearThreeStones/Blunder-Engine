## MODIFIED Requirements

### Requirement: Piercing menu activation

When the user Ctrl+right-clicks inside the editor 3D viewport over pickable geometry, the engine SHALL open a selection piercing menu listing pickable entities along the view ray through that pixel, ordered front-to-back, using **hierarchy-promoted** entity ids (immediate parent when present).

#### Scenario: Open piercing menu

- **WHEN** the user Ctrl+right-clicks a viewport pixel that intersects one or more pickable meshes
- **THEN** the engine SHALL display a popup menu at the cursor listing each distinct promoted hit entity by display name

#### Scenario: Parent and child collapse to parent row

- **WHEN** multiple child mesh renderers under the same parent appear in the broad hit list
- **THEN** the piercing menu SHALL show a single row for the promoted parent entity

### Requirement: Multi-hit broad-phase hit list

The piercing menu and same-pixel click cycling SHALL obtain multi-hit lists from the **GPU broad-phase hit list** (sorted by ray distance, promoted and deduplicated), not from iterative full-scene depth peel passes.

#### Scenario: Overlapping meshes listed in depth order

- **WHEN** multiple pickable mesh renderers overlap at the same viewport pixel along the view ray
- **THEN** the hit list SHALL include each distinct promoted entity ordered from nearest to farthest

#### Scenario: Alpha-cutout respected in narrow phase

- **WHEN** the front-most narrow-phase sample fails alpha-mask cutoff at the pixel
- **THEN** that surface SHALL NOT produce the primary click selection (broad list may still include behind surfaces)

#### Scenario: Blend meshes skipped

- **WHEN** only blend-transparent meshes exist along the ray at the pixel
- **THEN** the piercing menu SHALL NOT open (same as no hit)

## REMOVED Requirements

### Requirement: Multi-hit GPU depth peel

**Reason:** Replaced by broad-phase hit list + single candidate narrow pass (P2).

**Migration:** Remove `PickOverlay::pickAllAtWindowPosition` iterative peel loop; use `HybridGpuPickSystem` broad hit list with parent promotion and deduplication.
