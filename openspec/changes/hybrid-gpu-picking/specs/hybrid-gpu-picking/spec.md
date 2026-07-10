## ADDED Requirements

### Requirement: Async GPU pick lifecycle

The engine SHALL perform viewport mesh picking using an asynchronous GPU pick lifecycle with `requestPick` and `tryFetch` semantics, avoiding synchronous blocking of the main thread on pick fence completion during input dispatch.

#### Scenario: Request on click

- **WHEN** the user performs a viewport left-click that passes gizmo checks and click-vs-drag validation
- **THEN** the engine SHALL submit a GPU pick command buffer and return without waiting for readback in the input handler

#### Scenario: Fetch result next frame

- **WHEN** a pick was requested on a prior frame and the pick fence has signaled
- **THEN** `tryFetch` SHALL return the pick result and apply selection on that frame

#### Scenario: Pending fetch

- **WHEN** a pick is requested and the fence has not signaled yet
- **THEN** `tryFetch` SHALL report pending and SHALL NOT change selection

### Requirement: GPU pick instance buffer

The engine SHALL maintain a GPU-resident pick instance table for the active scene, containing per pickable mesh renderer: leaf `EntityId`, parent `EntityId`, world-space AABB, pickable flags, and data required for narrow-phase draw lookup.

#### Scenario: Rebuild on scene dirty

- **WHEN** the active scene's meshes, transforms, or pickable set changes
- **THEN** the instance buffer SHALL be rebuilt or incrementally updated before the next pick request uses it

#### Scenario: No per-click scene scan

- **WHEN** a pick is requested after P1 is enabled
- **THEN** the engine SHALL NOT traverse all mesh renderers on the CPU solely to build the pick draw list

### Requirement: Compute broad phase ray-AABB cull

The engine SHALL implement a compute shader broad phase that tests the viewport pick ray against each instance world AABB and appends hits to a GPU hit list capped at 1024 entries.

#### Scenario: Hit list append

- **WHEN** the broad phase dispatch completes for a pick ray
- **THEN** each instance whose world AABB intersects the ray SHALL be eligible for append to the hit list with an entry distance along the ray

#### Scenario: Hit list cap

- **WHEN** more than 1024 instances intersect the pick ray
- **THEN** the hit list SHALL contain at most 1024 entries and SHALL NOT crash or corrupt GPU memory

### Requirement: Candidate-only narrow phase

After broad phase, the engine SHALL rasterize the entity-ID pick prepass only for broad-phase candidate instances (not the full scene) and read back the front-most entity ID at the click pixel.

#### Scenario: Narrow confirms front-most among candidates

- **WHEN** multiple candidates overlap at the click pixel
- **THEN** the narrow phase depth test SHALL resolve the front-most leaf entity at that pixel

#### Scenario: Alpha-cutout in narrow phase

- **WHEN** a candidate surface fails alpha-mask cutoff at the sampled pixel
- **THEN** that surface SHALL NOT be selected

### Requirement: Hierarchy parent promotion

When a pick resolves to a leaf entity `L`, the engine SHALL promote the result to `L`'s immediate parent entity `P` when `P` is valid in the active scene (`Entity::getParentId()`).

#### Scenario: Child mesh promotes to parent

- **WHEN** a pick hits a leaf entity that has a valid parent entity in the hierarchy
- **THEN** the engine SHALL select the parent entity, not the leaf

#### Scenario: Root entity unchanged

- **WHEN** a pick hits a leaf entity with no parent (`k_invalid_entity_id`)
- **THEN** the engine SHALL select the leaf entity

#### Scenario: Promotion in piercing menu

- **WHEN** the piercing menu lists hit entities
- **THEN** each row SHALL represent the promoted entity (parent if present) and SHALL deduplicate duplicate promoted ids

### Requirement: Click versus drag discrimination

The engine SHALL NOT trigger viewport mesh pick on pointer release when the pointer has moved more than 5 pixels from the press origin during an active viewport camera manipulation gesture.

#### Scenario: Drag orbit does not pick

- **WHEN** the user drags to orbit or pan the viewport and releases the button after moving more than 5 pixels
- **THEN** the engine SHALL NOT apply a mesh pick from that release

#### Scenario: Stationary click picks

- **WHEN** the user presses and releases the left button within 5 pixels in the viewport without a qualifying drag
- **THEN** the engine SHALL request a mesh pick as normal
