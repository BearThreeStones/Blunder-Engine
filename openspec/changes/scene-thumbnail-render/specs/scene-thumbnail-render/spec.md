## ADDED Requirements

### Requirement: Scene Thumbnail Render still for Scene Assets
The system SHALL generate Content Browser Thumbnails for on-disk `.scene.asset` entries using **Scene Thumbnail Render**: a single still frame through a resolved scene Camera into a dedicated offscreen target (not the main viewport, not Camera Preview, not Mesh Preview as the product owner), written into the project thumbnail cache.

#### Scenario: Scene with Main Camera
- **WHEN** a `.scene.asset` with a Main Camera and at least one successful mesh attach is enqueued for thumbnail generation
- **THEN** the cache stores an RGBA still rendered through that Main Camera at square aspect

#### Scenario: Scene without Main Camera but with another Camera
- **WHEN** a `.scene.asset` has Cameras but none marked Main
- **THEN** Scene Thumbnail Render uses the first Camera in stable EntityId order (Play resolve rule)

#### Scenario: Scene with no Camera
- **WHEN** a `.scene.asset` has no Camera Component entities
- **THEN** thumbnail generation SHALL fall back to a Scene placeholder (not leave a blank tile as the success path)

### Requirement: On-disk source and childScenes
Scene Thumbnail Render SHALL instantiate from the on-disk Scene Asset (not a dirty live editor SceneInstance) and SHALL include recursive childScenes consistent with normal scene load. Camera resolve SHALL run on the root scene being thumbnailed, not on child scenes' cameras.

#### Scenario: Root camera sees child content
- **WHEN** the root scene references a childScene that contains mesh entities
- **THEN** those meshes appear in the still when viewed from the root-resolved camera (subject to frustum)

### Requirement: Cache invalidation fingerprint
Thumbnail cache validity for a Scene Asset SHALL depend on the scene file modification time and a fingerprint of direct Mesh Asset References on the root scene and recursive child scenes.

#### Scenario: Referenced mesh changes
- **WHEN** a direct Mesh Asset Reference used by the scene (or a child scene) changes on disk and the scene file mtime is unchanged
- **THEN** the prior Scene thumbnail cache entry is treated invalid and regenerated

### Requirement: No editor chrome or AnimationPlayer sampling
Scene Thumbnail Render SHALL NOT draw Editor Overlays and SHALL NOT sample AnimationPlayer clips for this slice (skinned content remains bind/rest pose).

#### Scenario: Overlay-free still
- **WHEN** a Scene thumbnail is generated
- **THEN** the image contains no gizmo, grid, or selection overlay pixels from the editor overlay systems
