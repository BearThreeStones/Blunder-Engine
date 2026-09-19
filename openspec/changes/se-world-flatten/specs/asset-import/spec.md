## ADDED Requirements

### Requirement: Import ignores instance_asset_id
glTF Import and runtime mesh attach SHALL ignore `instance_asset_id` extras on glTF nodes. They SHALL NOT look up `asset_index.json` at Import or load time, SHALL NOT create nested Scene Assets from those extras, and SHALL NOT fail Import solely because `instance_asset_id` is present. Expansion of those extras is the offline flatten bake, not Import.

#### Scenario: Importing SE-world glTF does not expand instances
- **WHEN** the user Imports `SE-world.gltf` through the normal Import path
- **THEN** Import does not write thousands of layout entities from `instance_asset_id`
- **AND** Import does not abort because those extras are present

#### Scenario: Attach does not consult asset_index
- **WHEN** scene instantiate attaches a Mesh Asset whose Intermediate glTF still has `instance_asset_id` extras
- **THEN** attach does not read `asset_index.json` to spawn child instances

### Requirement: Flatten skip does not abort other Imports
A skipped flatten instance (missing asset id or missing glTF file) SHALL NOT fail unrelated mesh Import in the same Project. Skip is per instance on the bake path.

#### Scenario: Other meshes still Import
- **WHEN** flatten skips `LI-bushlet_blue_delicate_006` for a missing asset id
- **AND** another library glTF in the same bake is present
- **THEN** that other library glTF can still Import as a Mesh Asset
