## ADDED Requirements

### Requirement: Delete invalidates Final for Mesh and Texture
When a Mesh or Texture Asset is deleted, the cook system SHALL mark that GUID's Final stale or remove cooked outputs so a later load does not treat a leftover Final as authoritative for a missing Asset.

#### Scenario: Cooked mesh path cleared after Mesh delete
- **WHEN** a Mesh Asset with a cooked Final is deleted successfully
- **THEN** subsequent load/cook paths do not serve that GUID's previous Final as a live Asset
