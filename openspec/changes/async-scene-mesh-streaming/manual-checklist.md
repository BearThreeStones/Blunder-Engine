# Manual checklist — async-scene-mesh-streaming

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Windowed `engine_editor` / `engine_player` on the DogWalk Project (`E:\Blunder Projects\DogWalk`). Open `assets/Scenes/se-world.scene.asset` on a GUID-bind build. Do not use Test or Sponza as the forest. Do not use collision QC `root.scene.asset`. Do not use the collision product exe that re-imports ~10k glTF documents.

| # | User story | Pass |
|---|------------|------|
| 1 | Open the DogWalk main scene `se-world.scene.asset`; the editor window can orbit and click. The first frame does not wait until all unique Mesh Assets are CPU+GPU resident. | |
| 2 | In the Viewport the forest grows as unique meshes become resident: eventually the real trees, fence, and ground GEO (including grass / bush / reed in that suite), not eight empty nodes and not placeholder boxes; units are metres, Z-up. | |
| 3 | Play (`engine_player`) enters the same scene: the first frame is also not blocked on all-mesh residency; when complete it draws the same forest (existing GPU-driven). | |
| 4 | If one unique Mesh fails, only that class is missing; the rest of the scene is not emptied. | |
| 5 | The main scene is in DogWalk; Test and Sponza are not the forest. Collision QC stays on the existing `root.scene.asset`. | |
| 6 | The dog does not need to walk on this ground. C# Find / group / ray and collision wireframe are not required. First-frame checkerboard albedo (Texture Loader still in flight) is allowed and is not a failure. | |
