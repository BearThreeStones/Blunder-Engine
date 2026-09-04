## ADDED Requirements

### Requirement: Pick skips Objects that are not Active in Hierarchy
Viewport mesh pick SHALL NOT hit an Object that is not Active in Hierarchy. Peel lists, same-pixel cycling, and the piercing menu SHALL omit those Objects.

#### Scenario: Inactive mesh is not picked
- **WHEN** the pointer is over a MeshRenderer that is not Active in Hierarchy
- **AND** the author left-clicks that pixel
- **THEN** that Object is not selected from that pick
