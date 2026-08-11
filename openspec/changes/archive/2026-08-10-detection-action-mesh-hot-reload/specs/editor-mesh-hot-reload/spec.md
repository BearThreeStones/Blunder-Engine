## ADDED Requirements

### Requirement: Mesh session hot reload after Reimport
After a successful Mesh Reimport (manual or Detection-driven), the editor SHALL refresh already-loaded Mesh presentation in the current session so AssetManager-backed viewport drawing, Mesh Preview, and scene-placed meshes that reference that Mesh GUID show updated Intermediate/Final data without restarting the editor. This requirement does not require AnimationClip or AnimationPlayer live track hot-swap, and does not require an immediate Cook solely to satisfy hot reload.

#### Scenario: Viewport mesh updates after Reimport
- **WHEN** a Mesh Asset that is visible in the editor viewport is successfully Reimported
- **THEN** subsequent editor frames draw using the refreshed Mesh data for that GUID without requiring an editor restart

#### Scenario: Mesh Preview updates after Reimport
- **WHEN** the Asset Inspector Mesh Preview is showing a Mesh that is successfully Reimported
- **THEN** the Mesh Preview reflects the refreshed Mesh data

#### Scenario: Clip Reimport does not require mesh hot reload
- **WHEN** only an AnimationClip Asset is Reimported
- **THEN** this capability does not require Mesh session hot reload as a side effect
