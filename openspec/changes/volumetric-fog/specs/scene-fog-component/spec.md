## Purpose

Authors place a Unique Fog Component in the Scene Asset so Player volumetric fog has an enable gate, height-density source, and view distance — not a hidden process-global fog rig.

## ADDED Requirements

### Requirement: Fog Component on scene entities
The engine SHALL support a native Fog Component on scene entities as a Unique attachment (at most one per entity). Pose SHALL follow that entity’s TRS. World **Z** of that entity SHALL be the fog height origin (Z-up). The Component SHALL NOT be a C# Behaviour. Adding Fog SHALL NOT create a bound Object.

#### Scenario: Round-trip Fog Component
- **WHEN** an entity has a Fog Component with volumetric enabled, density, height falloff, view distance, albedo, and scattering distribution
- **THEN** scene serialize and load preserve those fields

#### Scenario: Absent fog key
- **WHEN** an entity JSON object has no `"fog"` key
- **THEN** the loaded entity has no Fog Component

### Requirement: Fog fields and defaults
Every Fog Component SHALL have: Fog enabled (default on), volumetric fog enabled (default on), Fog density (default 0.02), Fog height falloff (default 0.2), Fog view distance (default 60 meters, positive), Fog albedo (linear RGB, default white), and scattering distribution `g` in `[-0.99, 0.99]` (default 0.2). Fog enabled off SHALL contribute no volumetric fog. Volumetric fog enabled off SHALL contribute no volumetric fog even if Fog enabled is on. This slice SHALL NOT store a second height-fog term, a box volume, or a 2D height-fog pass flag.

#### Scenario: Add Fog has visible defaults
- **WHEN** the author adds Fog to an entity that has none
- **THEN** that Fog Component has volumetric fog enabled, density 0.02, height falloff 0.2, view distance 60 meters, white albedo, and `g` 0.2

#### Scenario: Fog enabled off is ignored
- **WHEN** a Fog Component is Fog enabled off
- **THEN** it does not contribute volumetric fog

### Requirement: One active Fog per view
When more than one Fog Component exists, the engine SHALL use the first Fog that is Fog enabled, volumetric fog enabled, Active in Hierarchy, and has Fog view distance > 0, in stable EntityId order. Other Fog Components SHALL be ignored this slice.

#### Scenario: First Fog wins
- **WHEN** two Fog enabled volumetric Fog Components exist
- **THEN** only the lower EntityId Fog supplies density and view distance

#### Scenario: No Fog means no volumetric fog
- **WHEN** a scene has no Fog Component that meets the active-Fog rule
- **THEN** volumetric fog does not run
