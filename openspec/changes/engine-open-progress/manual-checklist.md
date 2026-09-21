# Manual checklist — engine-open-progress

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Windowed `engine_editor` on the DogWalk Project (`E:\Blunder Projects\DogWalk`). Open `assets/Scenes/se-world.scene.asset`. Do not use Test or Sponza. Do not use collision QC `root.scene.asset`. Do not use Player, Project Manager, Headless, CLI, or MCP as this overlay. Do not use PR 24 / collision product exe.

| # | User story | Pass |
|---|------------|------|
| 1 | Open DogWalk from Project Manager or Debug. After windowed `engine_editor` appears, and before the window can Iterate, a Godot red-box style center progress is visible: title, percent bar, current stage caption, and elapsed seconds. | |
| 2 | The Shell (Application Bar, docks, Viewport grid) may already be on screen. Progress is a modal overlay on that Shell, not a second splash window, and not the Startup cover held until the Live scene finishes. | |
| 3 | After the entity table is built and the window can Iterate, the overlay is gone. The forest may still grow (Mesh Loader); checkerboard albedo (Texture Loader) may still fly; Content Browser thumbnails may still be 2/tick. Those are not overlay leftover. | |
| 4 | Closing the window while the overlay is up ends this Editor Session. There is no Retry. | |
| 5 | Headless, `--mcp`, CLI, Project Manager, and Player have no overlay. | |
| 6 | The open target is the DogWalk main scene `se-world.scene.asset`. Test and Sponza are not the demo Project. The dog does not need to walk. Collision wireframe and C# Find are not required. | |
