## ADDED Requirements

### Requirement: Viewport left-click mesh selection

When the user presses the left mouse button inside the editor 3D viewport, the engine SHALL attempt to pick a scene entity by mesh intersection and update `EditorSelectionSystem` according to the active modifier keys.

#### Scenario: Pick opaque mesh entity

- **WHEN** the user left-clicks a viewport pixel that intersects an opaque mesh renderer in the active scene
- **THEN** the engine SHALL select the entity that owns that mesh renderer (leaf entity)

#### Scenario: Pick with no mesh hit

- **WHEN** the user left-clicks inside the viewport and the pick ray does not intersect any pickable mesh
- **THEN** the engine SHALL clear the editor selection

#### Scenario: Click outside viewport

- **WHEN** the user left-clicks outside the editor viewport rect
- **THEN** the viewport pick system SHALL NOT change selection

### Requirement: Selection modifier keys

Viewport picking SHALL use the same selection semantics as the Hierarchy panel: plain click replaces selection, Shift+click adds to selection, and Ctrl+click toggles selection membership.

#### Scenario: Replace selection

- **WHEN** the user left-clicks a mesh without Shift or Ctrl held
- **THEN** the engine SHALL call `EditorSelectionSystem::setSelection` with the picked entity

#### Scenario: Add to selection

- **WHEN** the user Shift+left-clicks a mesh
- **THEN** the engine SHALL call `EditorSelectionSystem::addToSelection` with the picked entity

#### Scenario: Toggle selection

- **WHEN** the user Ctrl+left-clicks a mesh
- **THEN** the engine SHALL call `EditorSelectionSystem::toggleSelection` with the picked entity

### Requirement: Input priority over viewport pick

Viewport mesh picking SHALL NOT run when a higher-priority editor interaction consumes the left-click event.

#### Scenario: Transform gizmo handle hit

- **WHEN** the user left-clicks a viewport pixel that hits an active transform gizmo handle
- **THEN** the transform gizmo controller SHALL handle the event and viewport mesh picking SHALL NOT change selection

#### Scenario: Navigate gizmo hit

- **WHEN** the user left-clicks a viewport pixel that hits the navigate gizmo
- **THEN** the navigate gizmo SHALL handle the event and viewport mesh picking SHALL NOT change selection

### Requirement: CPU raycast pick path (Scheme A)

The default pick implementation SHALL cast a world-space ray from the click position using `EditorCamera::makeRayFromWindowPosition`, broad-phase cull mesh candidates with a world-space AABB, and narrow-phase test ray-triangle intersection on CPU mesh data, returning the closest hit along the ray.

#### Scenario: Closest mesh wins

- **WHEN** the pick ray intersects multiple pickable meshes
- **THEN** the engine SHALL select the entity owning the mesh with the smallest positive intersection distance `t`

#### Scenario: Broad-phase cull

- **WHEN** a mesh renderer's world-space AABB does not intersect the pick ray
- **THEN** the CPU pick path SHALL NOT triangle-test that mesh

#### Scenario: Local AABB cache

- **WHEN** a `MeshAsset` is loaded or built
- **THEN** the engine SHALL compute and cache a local-space axis-aligned bounding box from mesh vertex positions for pick broad phase

### Requirement: Alpha-cutout pick parity

Meshes using alpha mask / alpha cutoff (`cgltf_alpha_mode_mask` or equivalent) SHALL be pickable only where the surface would be visible in the forward pass.

#### Scenario: Cutout hole not pickable

- **WHEN** the pick ray intersects a triangle whose interpolated alpha is below the mesh renderer's `alpha_cutoff`
- **THEN** that triangle SHALL NOT count as a hit

### Requirement: Blend-transparent meshes excluded from pick

Mesh renderers using blend transparency (`cgltf_alpha_mode_blend`) SHALL NOT participate in viewport picking in v1.

#### Scenario: Click through transparent mesh

- **WHEN** the user clicks a pixel where only blend-transparent meshes are visible along the ray
- **THEN** the pick SHALL behave as a miss unless an opaque or cutout mesh lies behind them along the ray

### Requirement: GPU ID pick path (Scheme B)

The engine SHALL provide an optional GPU pick path that renders all pickable meshes into an entity-ID render target with depth, using the same view/projection as the viewport, and resolves a pick by reading the entity ID at the click pixel.

#### Scenario: GPU pick hit

- **WHEN** the GPU pick path is active and the user left-clicks inside the viewport over a pickable mesh
- **THEN** the sampled entity ID at the click pixel SHALL match the same leaf entity that the CPU path would return for that pixel (within depth and alpha-cutout rules)

#### Scenario: GPU pick miss

- **WHEN** the GPU pick path is active and the click pixel has no pickable mesh ID written
- **THEN** the engine SHALL clear selection (same as CPU miss)

### Requirement: Dual-path selection strategy

The editor SHALL use the CPU raycast path by default and MAY switch to the GPU ID pick path when configured or when scene triangle count exceeds a documented threshold.

#### Scenario: Default CPU path

- **WHEN** no GPU-pick override is configured and scene complexity is below the auto-fallback threshold
- **THEN** viewport picks SHALL be resolved by the CPU raycast path only

#### Scenario: GPU path override

- **WHEN** `BLUNDER_VIEWPORT_GPU_PICK=1` is set
- **THEN** viewport picks SHALL be resolved by the GPU ID pick path

#### Scenario: CPU triangle budget fallback

- **WHEN** the active scene's pickable triangle count exceeds the configured CPU budget (default: 500000 triangles)
- **THEN** the engine SHALL use the GPU ID pick path for that frame's pick resolution

### Requirement: Pick integration with editor services

A successful viewport pick SHALL trigger the same downstream updates as a Hierarchy selection change (viewport redraw, inspector/hierarchy dirty flags as applicable).

#### Scenario: Pick triggers redraw

- **WHEN** a viewport pick changes the editor selection
- **THEN** the render system SHALL request a viewport redraw
