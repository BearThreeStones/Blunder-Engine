# Manual checklist — shader-resource-layout

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Windowed `engine_editor` on the Test Project (`E:\Blunder Projects\Test`). Story 4 may use Headless or a locally broken Engine shader.

| # | User story | Pass |
|---|------------|------|
| 1 | Open the Test Project; the editor viewport shows the usual PBR scene (opaque, transparent if present, shadows if enabled, skinned if present) with textures in the right slots. | |
| 2 | Open a Mesh Asset that has material textures in Content Browser; Mesh Preview still auto-frames and shows the correct surface. | |
| 3 | Ground grid, Transform gizmo, and Navigate gizmo still draw in the viewport (they use the same VulkanPipeline class). | |
| 4 | Deliberately make an Engine shader’s binding set disagree with what the draw path writes: the process FATAL before a viewport appears, and does not run with a wrong layout. (Headless / a local broken shader is enough.) | |
