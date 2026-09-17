# Manual checklist — secondary-command-buffers

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–3 are windowed Editor (scene with shadows, SSAO, overlays). Stories 4–5 are quit / Headless.

| # | User story | Pass |
|---|------------|------|
| 1 | I open a scene with materials, shadows, SSAO, and editor overlays (grid, outline, gizmos): it looks like today, and the viewport still presents. | |
| 2 | I orbit the camera: there is no extra hitch; Bindless fallback then real textures still work; shadows, SSAO, and overlays stay stable. | |
| 3 | I use Camera Preview and Mesh Preview: both still draw (same forward path, no authorship overlays in those views). | |
| 4 | I quit the Editor or Player while a frame is still in flight: the process exits and does not hang resetting secondaries. | |
| 5 | I start Headless Editor or Headless Player with no Vulkan device: no secondary buffers are allocated, and the process still starts and exits. | |
