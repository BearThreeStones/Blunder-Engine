## ADDED Requirements

### Requirement: Product delete unregisters GUID
When Content Browser Asset Delete succeeds, the AssetRegistry SHALL no longer resolve the deleted GUID, and registry persistence SHALL reflect the removal.

#### Scenario: GUID unresolvable after delete
- **WHEN** Delete completes for GUID G
- **THEN** `resolveGuid(G)` fails / returns empty and a subsequent registry load does not restore G unless re-imported
