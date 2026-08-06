## ADDED Requirements

### Requirement: Content Browser can delete a selected Asset
The Content Browser SHALL allow the user to select an Asset descriptor in the grid and request Delete (keyboard Delete and/or context menu). Delete SHALL NOT apply to folder rows.

#### Scenario: Delete removes Mesh descriptor from browser
- **WHEN** the user selects `assets/Meshes/Chocomel.mesh.yaml` and confirms Delete, and no other Asset depends on its GUID
- **THEN** the descriptor file is gone, the GUID is unregistered, and Content Browser refresh no longer lists that Asset

#### Scenario: Delete detaches Scene dependents then removes Asset
- **WHEN** the user requests Delete for an Asset whose only dependents are Scene Assets referencing its GUID
- **THEN** those Scene entity mesh/clip references are cleared on disk, the descriptor is removed, and the GUID is unregistered

#### Scenario: Delete refused when non-scene dependents exist
- **WHEN** the user requests Delete for an Asset that is still referenced by another non-Scene Asset (for example a Mesh referencing a Texture)
- **THEN** Delete does not remove the descriptor or unregister the GUID, and the user is informed which dependents block delete

#### Scenario: Multi-select Delete removes each selected Asset
- **WHEN** the user Ctrl/Shift-selects multiple Asset descriptors and requests Delete
- **THEN** each selected non-folder Asset is deleted independently (scene dependents detached as for single Delete); folders in the selection are skipped; Content Browser refreshes after the batch

#### Scenario: Folder is not deletable via Asset Delete
- **WHEN** the selection is a folder entry
- **THEN** Asset Delete does not run (no descriptor/registry mutation)

### Requirement: Delete cleans Intermediate and Final best-effort
Asset Delete SHALL remove the descriptor Intermediate `source` body when present, mark Mesh/Texture Finals stale or remove cooked outputs for that GUID, and SHALL NOT require a fresh Cook to succeed for Delete itself to complete.

#### Scenario: Mesh Intermediate source removed
- **WHEN** a Mesh Asset with `source: resources/Models/Chocomel/Chocomel.gltf` is deleted successfully
- **THEN** that Intermediate glTF (and best-effort listed companion Intermediate bodies on the Mesh descriptor) are removed from Resources

#### Scenario: AnimationClip Intermediate removed without Final cook
- **WHEN** an AnimationClip Asset is deleted successfully
- **THEN** its descriptor and Intermediate clip YAML under Resources are removed; no Final cook path is required

### Requirement: Mesh Delete does not cascade AnimationClip Assets
Deleting a Mesh SHALL NOT unregister or delete AnimationClip Assets that were extracted from companion glTFs, even if those companions were listed on `companion_animation_sources`.

#### Scenario: Clips survive Mesh delete
- **WHEN** a Mesh with companion-derived clips is deleted successfully
- **THEN** those AnimationClip descriptors remain registered until deleted explicitly
