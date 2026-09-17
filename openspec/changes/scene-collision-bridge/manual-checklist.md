# Manual checklist — scene-collision-bridge

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Agent QC run 2026-09-17 (not human acceptance)

Agent QC (not acceptance): this cloud-agent session has **no** Cursor `blunder-editor` namespace. Spoke JSON-RPC stdio to Debug `engine_editor_qc.exe --mcp --project-root "E:\Blunder Projects\DogWalk" --scene assets/Scenes/root.scene.asset` (20 tools). `tools/call` JSON framing was missing a `}` (fixed on this branch; `machine_adapter_test` pass). Cursor `mcp.json` `--scene Assets/...` does not match the `assets/` virtual-path prefix, so that spelling never opens a live document.

Windowed `engine_editor` on the DogWalk Project (`E:\Blunder Projects\DogWalk`). Do not use Test or Sponza as the main scene. Agent QC used Headless `--mcp` stdio (same verbs); live `capture` returned `capture.scene_unreadable`; `play-frame` is a blank 1280×720 (no mesh renderers). Human still confirms.

| # | User story | Pass |
|---|------------|------|
| 1 | A static mesh can carry a static triangle-mesh collider; if that mesh has no trimesh data, it does not collide. | fail |
| 2 | An entity can take a Unique box, sphere, or capsule collider; anything that moves does not use trimesh. | pass |
| 3 | C# and the scene can raycast and sphere/capsule shapecast (box shapecast included), filter by layer mask, and read groups on the hit. | pass |
| 4 | The character moves with capsule sweep + slide / floor stick; writing `Object.Position` is not the walk path and does not tunnel through the world. | fail |
| 5 | The Viewport shows collision wireframes; edit-mode rays use the same query path as Play. | fail |
| 6 | Work lands in the DogWalk project; Test and Sponza are not the main scene. | pass |
