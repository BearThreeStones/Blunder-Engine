## ADDED Requirements

### Requirement: Browser rename and reparent rewrite registry paths
When a Content Browser rename or reparent moves an Asset Descriptor under the Assets root, the Asset Registry SHALL map the same GUID to the new descriptor virtual path and SHALL persist that mapping. Intermediate `source` and Source `archived_source` files SHALL NOT be required to move. Asset References by GUID SHALL still resolve.

#### Scenario: Rename updates registry path
- **WHEN** GUID G is registered at `assets/Chars/Hero.mesh.yaml` and the author Asset-renames the stem to `Villain`
- **THEN** resolving G returns `assets/Chars/Villain.mesh.yaml`
- **AND** `findGuidForPath` of the old path does not return G

#### Scenario: Folder reparent remaps nested GUIDs
- **WHEN** two descriptors under `assets/Chars/` are registered and the author reparents `assets/Chars/` to `assets/Enemies/`
- **THEN** both GUIDs resolve to paths under `assets/Enemies/Chars/`
