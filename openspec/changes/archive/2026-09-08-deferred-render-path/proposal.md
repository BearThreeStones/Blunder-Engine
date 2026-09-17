## Why

The editor viewport still lights every opaque draw in `ForwardRenderPath`. A **Deferred Render Path** (G-buffer, then screen-space lighting) is the next mesh shading path: same Light Components and linking, without waiting for Frame graph GPU or raising the 8-light cap.

## What Changes

- Add a hardcoded **Deferred Render Path** beside the Forward Render Path. Frame graph stays CPU-only. This is not clustered lighting and not a visibility buffer.
- Editor viewport opt-in via `BLUNDER_EDITOR_DEFERRED=1` (default off). Camera Preview, Mesh Preview, Scene Thumbnail / Capture, and Player stay on the Forward Render Path and ignore that env.
- Geometry writes an extra G-buffer (lean PBR + G-buffer receiver id) and the existing viewport depth. Lighting writes the existing offscreen color. SSAO, outline, pick, and gizmos keep that color/depth.
- Lighting uploads a **Deferred light list** (at most 32, EntityId order) and still applies Light linking plus the Light evaluation cap (8) per receiver. Opaque skinned draws write the G-buffer. Blend-transparent stays a Forward after-pass. Scene overlays run after lighting and before transparent. The existing Directional shadow map is sampled in lighting.
- First-party test covers Deferred light list + per-receiver 8 after linking vs `gatherLightsForMesh`, including the 33rd light dropped from the list.
- Decision record: [ADR 0062](../../../docs/adr/0062-deferred-render-path.md).

## User stories

1. 不设 `BLUNDER_EDITOR_DEFERRED` 时，编辑器主视口仍是 Forward；Camera Preview、Mesh Preview、Scene Thumbnail、Player 即使设了该变量也不走 Deferred。
2. 打开该变量后，主视口不透明（含蒙皮、unlit、alpha clip）的灯、linking、每 mesh 8 灯与 Forward 一致；空像素仍是视口背景。
3. 打开后，半透明仍在 grid 之后用 Forward 着色；alpha clip 仍在几何里打洞。
4. 打开后，现有 Directional 阴影、SSAO、outline、pick、gizmo 仍吃同一张 offscreen color/depth。
5. 一等测试：Deferred light list（上限 32）再按 receiver 做 linking + 8 灯，与 `gatherLightsForMesh` 一致；第 33 盏灯进不了 list。

## Capabilities

### New Capabilities

- `deferred-render-path`: Editor-viewport opt-in Deferred Render Path (G-buffer + lighting into existing offscreen color/depth); Forward remains default and the path for other mesh shading surfaces.

### Modified Capabilities

- `scene-light-component`: Deferred lighting uses the same Light linking and Light evaluation cap per receiver; Deferred light list upload cap is 32 and is not that evaluation cap.
- `secondary-command-buffers`: Deferred viewport records G-buffer and lighting as SECONDARY passes; scene overlay then transparent still run after lighting. Unset env keeps today’s one-subpass forward color order.
- `bindless-texture-table`: G-buffer geometry is a mesh shading path and uses the table. The lighting pass does not.

## Impact

- **Engine:** new `DeferredRenderPath` (or equivalent) under `engine/src/runtime/function/render/`; G-buffer targets; geometry + lighting Engine shaders; `RenderSystem` viewport branch; SecondaryPass slots for G-buffer and lighting; reuse `gatherLightsForMesh` / shadow map / Bindless / OverlaySystem / SsaOPass.
- **Tests:** first-party test for Deferred light list + per-receiver cap (story 5). Windowed stories 1–4 on the checklist.
- **Docs:** CONTEXT (Grill); ADR 0062; `docs/agents/render-pipeline.md` pass list and `BLUNDER_EDITOR_DEFERRED`.
- **Non-goals:** Frame graph GPU; replacing Forward as the editor default; Mesh Preview / Camera Preview / Thumbnail / Player on this path; clustered lighting; visbuffer; HDR / tone-map; Point/Spot/Area shadows; a product settings UI; raising the Light evaluation cap; MSAA; OIT.
