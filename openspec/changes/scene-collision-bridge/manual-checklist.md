# Manual checklist — scene-collision-bridge

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Agent QC re-run 2026-09-18 (not human acceptance)

Agent QC (not acceptance): this session has **no** Cursor `blunder-editor` namespace. Spoke JSON-RPC stdio to Debug `engine_editor_qc.exe --mcp --project-root "E:\Blunder Projects\DogWalk" --scene assets/Scenes/root.scene.asset` (21 tools, including `shapecast` / `collider` / `capture` / `play-frame`). Cursor `mcp.json` `--scene Assets/...` does not match the `assets/` virtual-path prefix; QC used `assets/Scenes/root.scene.asset`.

Windowed `engine_editor` on the DogWalk Project (`E:\Blunder Projects\DogWalk`). Do not use Test or Sponza as the main scene. Headless `--mcp` stdio used the same verbs. Live `capture` and `play-frame` now return cyan collision overlays (edit 1280×720, 20100 cyan px; play-frame 10961 cyan px). Human still confirms.

| # | User story | Pass |
|---|------------|------|
| 1 | A static mesh can carry a static triangle-mesh collider; if that mesh has no trimesh data, it does not collide. | pass |
| 2 | An entity can take a Unique box, sphere, or capsule collider; anything that moves does not use trimesh. | pass |
| 3 | C# and the scene can raycast and sphere/capsule shapecast (box shapecast included), filter by layer mask, and read groups on the hit. | pass |
| 4 | The character moves with capsule sweep + slide / floor stick; writing `Object.Position` is not the walk path and does not tunnel through the world. | pass |
| 5 | The Viewport shows collision wireframes; edit-mode rays use the same query path as Play. | pass |
| 6 | Work lands in the DogWalk project; Test and Sponza are not the main scene. | pass |
