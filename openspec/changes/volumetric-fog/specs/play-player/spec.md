## ADDED Requirements

### Requirement: Player color target includes volumetric fog
When the Player’s loaded scene has an active Fog Component, the Player color target used for the windowed game view and for Headless Play frames SHALL include the volumetric-fog Forward composite. When no active Fog Component is present, that color target SHALL match the unfogged Forward result.

#### Scenario: Fogged Play view
- **WHEN** the Player is running a scene with an active Fog Component
- **THEN** the presented Player color (or Headless Play frame) includes volumetric fog

#### Scenario: No Fog stays unfogged
- **WHEN** the Player is running a scene with no active Fog Component
- **THEN** the Player color target has no volumetric-fog composite
