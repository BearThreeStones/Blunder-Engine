# Manual checklist — se-world-flatten

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Windowed `engine_editor` on the DogWalk Project (`E:\Blunder Projects\DogWalk`). Open `assets/Scenes/se-world.scene.asset`. Do not use Test or Sponza as the forest. Do not treat collision QC `root.scene.asset` as this scene.

| # | User story | Pass |
|---|------------|------|
| 1 | Open the DogWalk main scene; the Viewport shows SE-world’s real trees, fence, and ground (including grass / bush / reed in that suite), not eight empty instance nodes and not placeholder boxes. | |
| 2 | One flat `.scene.asset`: about 5000 layout instances are each an entity; meshes reference shared library glTF / Mesh Assets; set nodes may use `parent` for grouping. | |
| 3 | Ground / path / snow / pond / creek enter as set-glTF GEO (and the same class of visible mesh); units are metres, Z-up. | |
| 4 | Duplicate names are uniquified; instances whose asset id is missing are skipped; a failed instance does not empty the whole scene. | |
| 5 | The main scene is in DogWalk; Test and Sponza are not the forest. Collision QC stays on the existing `root.scene.asset`. | |
| 6 | Play and the editor both draw this forest (existing GPU-driven mesh submission). The dog does not need to walk on this ground. C# Find / group / ray are not required. | |
