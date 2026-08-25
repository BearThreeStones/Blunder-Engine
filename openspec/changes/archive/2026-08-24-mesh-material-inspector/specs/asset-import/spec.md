## ADDED Requirements

### Requirement: Mesh Reimport keeps Mesh material override
Mesh Reimport SHALL rebuild Intermediate and the Import-built MaterialAsset and SHALL keep the Mesh material override bag on that Mesh descriptor. Reimport SHALL NOT delete override keys and SHALL NOT treat Reset overrides as Reimport.

#### Scenario: Reimport preserves Shininess override
- **WHEN** the Mesh descriptor stores an override Shininess key
- **AND** Mesh Reimport completes
- **THEN** that Shininess key is still on the descriptor
- **AND** load still overlays it on the rebuilt Import MaterialAsset
