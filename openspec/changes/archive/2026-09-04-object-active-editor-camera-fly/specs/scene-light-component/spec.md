## ADDED Requirements

### Requirement: Lights require Active in Hierarchy
A Light Component SHALL contribute neither illumination nor shadows unless its Object is Active in Hierarchy. That gate SHALL apply in addition to Light enabled. Such a light SHALL NOT occupy a slot in the Light evaluation cap and SHALL NOT be chosen as the shadow-casting Directional.

#### Scenario: Inactive light is ignored
- **WHEN** a Light enabled Directional's Object is Object Active off
- **THEN** it does not illuminate and does not cast Light shadows

#### Scenario: Inactive parent hides child light
- **WHEN** a Light enabled child is Object Active on
- **AND** its parent is Object Active off
- **THEN** that Light does not illuminate
