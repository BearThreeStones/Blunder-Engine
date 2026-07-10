## MODIFIED Requirements

### Requirement: Multi-hit GPU depth peel

The piercing menu SHALL obtain its hit list using GPU entity-ID prepass depth peeling at the click pixel, returning distinct leaf entities in front-to-back order. Peel passes SHALL use the same **zero-to-one depth convention** as the editor camera (`glm::perspectiveZO` / Vulkan clip space: nearer surfaces have **greater** stored depth values than farther surfaces).

#### Scenario: Overlapping meshes listed in depth order

- **WHEN** multiple pickable mesh renderers overlap at the same viewport pixel along the view ray
- **THEN** the piercing menu hit list SHALL include each distinct entity ordered from nearest to farthest

#### Scenario: Second peel pass finds behind surface

- **WHEN** a first peel pass records a front-most hit at depth `d₀` and a second pickable surface lies behind it along the ray
- **THEN** the peel chain SHALL append that behind-surface entity id (after parent promotion and deduplication) rather than terminating after one hit

#### Scenario: Alpha-cutout respected in peel passes

- **WHEN** a peeled pick pass encounters alpha-mask geometry below cutoff at the sampled pixel
- **THEN** that surface SHALL NOT produce a hit for that peel iteration

#### Scenario: Blend meshes skipped

- **WHEN** only blend-transparent meshes exist along the ray at the pixel
- **THEN** the piercing menu SHALL NOT open (same as no hit)
