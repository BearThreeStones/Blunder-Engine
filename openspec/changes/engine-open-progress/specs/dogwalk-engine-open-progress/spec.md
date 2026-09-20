# Spec Delta

## Purpose

Accept Editor open progress on DogWalk’s forest Scene Asset so a human can read stage, percent, and elapsed seconds after the Startup cover, then Iterate while meshes, textures, and thumbs may still fly.

## ADDED Requirements

### Requirement: Acceptance scene is DogWalk se-world in windowed editor
Human acceptance for this change SHALL open DogWalk `Assets/Scenes/se-world.scene.asset` (virtual path `assets/Scenes/se-world.scene.asset`) in windowed `engine_editor`. Test Project scenes and Sponza SHALL NOT stand in. Collision QC SHALL remain on `Assets/Scenes/root.scene.asset`. The walk SHALL NOT use Project Manager chrome, Player splash, or a Headless / CLI / MCP session as this overlay.

#### Scenario: Forest editor is the walk target
- **WHEN** the human walks this change
- **THEN** the windowed Live document is DogWalk `se-world.scene.asset`
- **AND** it is not a Test or Sponza scene
- **AND** it is not collision QC `root.scene.asset`
- **AND** the process is windowed `engine_editor`

### Requirement: Overlay is visible after cover and before Iterate
After the Startup cover yields and before the window can orbit and click without the modal, the human SHALL see the center Editor modal: title Opening editor, a percent bar, Indexing content or Opening scene, and elapsed seconds that include cover time.

#### Scenario: Red-box progress on the Shell
- **WHEN** the author opens DogWalk in windowed `engine_editor` and the Shell is on screen while instantiate or index scan still runs
- **THEN** the overlay is visible on that Shell
- **AND** it is not a second splash window
- **AND** the Startup cover is not still covering the client

### Requirement: Overlay is gone when Iterate can run; forest may still grow
When the entity table is instantiated and Content index refresh has finished, the overlay SHALL be gone. Unique Mesh residency, Texture Loader copies, and thumbnail 2/tick MAY continue. Those SHALL NOT fail this walk. The dog standing on this ground, C# Find / group / ray, and collision wireframe SHALL NOT be required.

#### Scenario: Iterate without the modal
- **WHEN** startup entities exist and Content index refresh has finished
- **THEN** the overlay is gone
- **AND** the author can Iterate the Viewport
- **AND** trees MAY still appear via Mesh Loader
- **AND** checkerboard albedo or unfinished thumbs MAY remain

### Requirement: Collision bridge git stays untouched
This change SHALL NOT edit `cursor/scene-collision-bridge-8a89` or [#24](https://github.com/BearThreeStones/Blunder-Engine/pull/24).

#### Scenario: Walk is not the collision PR
- **WHEN** the human walks Editor open progress
- **THEN** the session is not required to merge or rebuild PR 24
- **AND** collision QC `root.scene.asset` is unchanged by this overlay
