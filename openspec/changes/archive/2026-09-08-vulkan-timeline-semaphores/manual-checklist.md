# Manual checklist — vulkan-timeline-semaphores

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–3 are windowed Editor (scene with shadows, SSAO, overlays; Camera Preview / Mesh Preview; GPU pick). Stories 4–5 are quit / Headless.

| # | User story | Pass |
|---|------------|------|
| 1 | I open a scene with materials, shadows, SSAO, and editor overlays (grid, outline, gizmos): it looks like today, and the viewport still presents (readback or zero-copy still about one frame behind). | |
| 2 | I orbit the camera: there is no extra hitch; Bindless fallback then real textures still work. | |
| 3 | I use Camera Preview and Mesh Preview: both still draw. I click a mesh: GPU pick still returns a result on a later frame without stalling the tick. | |
| 4 | I quit the Editor or Player while a frame or an immediate upload is still in flight: the process exits and does not hang waiting on the timeline. | |
| 5 | I start Headless Editor or Headless Player with no Vulkan device: no timeline semaphore is allocated, and the process still starts and exits. | |
