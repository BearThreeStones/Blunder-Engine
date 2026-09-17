# Manual checklist — deferred-render-path

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Story 5 is a Headless Test run (`light_eval_test`). Stories 1–4 are windowed editor viewport (env on/off). Test Project: any scene with Light Components, opaque, skinned, blend-transparent, and the pick_test stack as needed.

| # | User story | Pass |
|---|------------|------|
| 1 | 不设 `BLUNDER_EDITOR_DEFERRED` 时，编辑器主视口仍是 Forward；Camera Preview、Mesh Preview、Scene Thumbnail、Player 即使设了该变量也不走 Deferred。 | |
| 2 | 打开该变量后，主视口不透明（含蒙皮、unlit、alpha clip）的灯、linking、每 mesh 8 灯与 Forward 一致；空像素仍是视口背景。 | |
| 3 | 打开后，半透明仍在 grid 之后用 Forward 着色；alpha clip 仍在几何里打洞。 | |
| 4 | 打开后，现有 Directional 阴影、SSAO、outline、pick、gizmo 仍吃同一张 offscreen color/depth。 | |
| 5 | 一等测试：Deferred light list（上限 32）再按 receiver 做 linking + 8 灯，与 `gatherLightsForMesh` 一致；第 33 盏灯进不了 list。 | |
