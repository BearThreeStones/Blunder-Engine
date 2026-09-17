# Manual checklist — scene-collision-bridge

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Windowed `engine_editor` on the DogWalk Project (`E:\Blunder Projects\DogWalk`). Do not use Test or Sponza as the main scene.

| # | User story | Pass |
|---|------------|------|
| 1 | A static mesh can carry a static triangle-mesh collider; if that mesh has no trimesh data, it does not collide. | |
| 2 | An entity can take a Unique box, sphere, or capsule collider; anything that moves does not use trimesh. | |
| 3 | C# and the scene can raycast and sphere/capsule shapecast (box shapecast included), filter by layer mask, and read groups on the hit. | |
| 4 | The character moves with capsule sweep + slide / floor stick; writing `Object.Position` is not the walk path and does not tunnel through the world. | |
| 5 | The Viewport shows collision wireframes; edit-mode rays use the same query path as Play. | |
| 6 | Work lands in the DogWalk project; Test and Sponza are not the main scene. | |
