# Manual checklist — async-texture-loader

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–3 are windowed Editor (textured scene). Stories 4–5 are quit / Headless. Test Project with several large material textures is required for 1–3.

| # | User story | Pass |
|---|------------|------|
| 1 | I open a scene with several large material textures: the first frames show the Bindless fallback, then the real textures appear, and the viewport does not freeze on decode or `vkWaitForFences`. | |
| 2 | I cause the same texture path to be requested again while it is still in flight: the engine uploads it once. | |
| 3 | I switch scenes while uploads are still in flight: the process does not crash, and completed copies do not write GPU images that were dropped with the old scene. | |
| 4 | I quit the Editor or Player while uploads are still in flight: the process exits without hanging on fence wait or Worker join. | |
| 5 | I start Headless Editor or Headless Player: there is no Vulkan upload when there is no device, and the process still exits cleanly. | |
