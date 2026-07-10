## MODIFIED Requirements

### Requirement: Viewport left-click mesh selection

When the user presses the left mouse button inside the editor 3D viewport, the engine SHALL pick the front-most pickable scene entity at the click pixel using the hybrid GPU pick pipeline and update `EditorSelectionSystem` according to the active modifier keys. The selected entity SHALL be the **immediate hierarchy parent** of the struck leaf mesh entity when that parent exists.

#### Scenario: Pick promotes to parent entity

- **WHEN** the user left-clicks a viewport pixel that intersects a child mesh renderer whose entity has a valid parent in the scene hierarchy
- **THEN** the engine SHALL select the parent entity via GPU pick (not the leaf mesh entity)

#### Scenario: Pick root mesh entity

- **WHEN** the user left-clicks a viewport pixel that intersects a mesh renderer on a root entity (no parent)
- **THEN** the engine SHALL select that leaf entity

#### Scenario: Pick with no mesh hit

- **WHEN** the user left-clicks inside the viewport and the GPU pick pass finds no pickable mesh at the pixel
- **THEN** the engine SHALL clear the editor selection

#### Scenario: Click outside viewport

- **WHEN** the user left-clicks outside the editor viewport rect
- **THEN** the viewport pick system SHALL NOT change selection

#### Scenario: Async pick latency

- **WHEN** the user left-clicks to pick a mesh
- **THEN** selection MAY update on the next frame after fence completion and SHALL NOT block the main thread waiting for GPU readback in the input handler

### Requirement: GPU ID pick path (Scheme B)

The engine SHALL use a hybrid GPU pick pipeline (async fence, GPU instance buffer, compute broad phase, candidate narrow raster) as the sole viewport pick implementation. The front-most hit resolves by sampling the entity-ID buffer at the click pixel after narrow-phase rasterization of broad-phase candidates.

#### Scenario: GPU pick hit

- **WHEN** the user left-clicks inside the viewport over a pickable mesh
- **THEN** the sampled entity ID at the click pixel SHALL identify the front-most leaf entity along the view ray (respecting depth and alpha-cutout rules), then promote to parent when applicable

#### Scenario: GPU pick miss

- **WHEN** the user left-clicks a viewport pixel with no pickable mesh ID written
- **THEN** the engine SHALL clear selection

## REMOVED Requirements

### Requirement: Synchronous immediate-command pick

**Reason:** Superseded by async fence pick (P0) to avoid main-thread GPU stalls.

**Migration:** Replace `PickOverlay` synchronous `beginImmediateCommands` readback with `HybridGpuPickSystem::requestPick` / `tryFetch`.

### Requirement: Full-scene narrow raster every click (post-P2)

**Reason:** Superseded by candidate-only narrow phase after compute broad phase.

**Migration:** Narrow phase draws only broad-phase candidates (≤1024 instances).
