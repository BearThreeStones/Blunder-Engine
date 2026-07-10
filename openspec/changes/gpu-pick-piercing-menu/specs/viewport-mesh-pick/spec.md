## MODIFIED Requirements

### Requirement: Viewport left-click mesh selection

When the user presses the left mouse button inside the editor 3D viewport, the engine SHALL pick the front-most pickable scene entity at the click pixel using the GPU entity-ID pick pass and update `EditorSelectionSystem` according to the active modifier keys.

#### Scenario: Pick opaque mesh entity

- **WHEN** the user left-clicks a viewport pixel that intersects an opaque mesh renderer in the active scene
- **THEN** the engine SHALL select the entity that owns that mesh renderer (leaf entity) via GPU pick

#### Scenario: Pick with no mesh hit

- **WHEN** the user left-clicks inside the viewport and the GPU pick pass finds no pickable mesh at the pixel
- **THEN** the engine SHALL clear the editor selection

#### Scenario: Click outside viewport

- **WHEN** the user left-clicks outside the editor viewport rect
- **THEN** the viewport pick system SHALL NOT change selection

### Requirement: GPU ID pick path (Scheme B)

The engine SHALL use a GPU entity-ID prepass as the **sole** viewport pick implementation. All pickable meshes render into an entity-ID + depth target with the viewport view/projection; the front-most hit resolves by sampling the buffer at the click pixel.

#### Scenario: GPU pick hit

- **WHEN** the user left-clicks inside the viewport over a pickable mesh
- **THEN** the sampled entity ID at the click pixel SHALL identify the front-most leaf entity along the view ray (respecting depth and alpha-cutout rules)

#### Scenario: GPU pick miss

- **WHEN** the user left-clicks a viewport pixel with no pickable mesh ID written
- **THEN** the engine SHALL clear selection

## REMOVED Requirements

### Requirement: CPU raycast pick path (Scheme A)

**Reason:** Superseded by GPU-only picking for consistency and performance on dense scenes.

**Migration:** All viewport picks use `PickOverlay` / `RenderSystem::pickEntityAtWindowPosition`. Remove `ViewportPickSystem::pickCpu` and ray-triangle pick iteration.

### Requirement: Dual-path selection strategy

**Reason:** Single GPU path only; no CPU fallback or env toggle.

**Migration:** Remove `BLUNDER_VIEWPORT_GPU_PICK`, `k_cpu_pick_triangle_budget`, `shouldUseGpuPick`, and triangle-count cache from `ViewportPickSystem`.

#### Scenario: Default CPU path

**Reason:** Removed with dual-path strategy.

**Migration:** N/A — GPU is always used.

#### Scenario: GPU path override

**Reason:** GPU is no longer optional.

**Migration:** Remove `BLUNDER_VIEWPORT_GPU_PICK` documentation and code.

#### Scenario: CPU triangle budget fallback

**Reason:** No CPU path exists.

**Migration:** N/A.

### Requirement: Local AABB cache (pick broad phase)

**Reason:** Local AABB was required only for CPU pick broad phase.

**Migration:** `MeshAsset::getLocalBounds()` MAY remain for other systems (camera framing, culling follow-ups) but is no longer a pick requirement.

#### Scenario: Local AABB cache

**Reason:** Not used by GPU pick.

**Migration:** No pick dependency on AABB cache.
