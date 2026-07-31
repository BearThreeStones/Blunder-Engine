## Why

Content Browser Mesh thumbnails currently show a material **base-color texture** (or a placeholder), not a framed 3D still — so assets like Sponza read as foliage atlases instead of meshes. Authors also have no Inspector way to orbit a Mesh Asset without placing it in the scene. Authorship needs a shared **Mesh Preview Render** path for both thumbnails and interactive preview.

## What Changes

- Introduce **Mesh Preview Render**: dedicated offscreen draw of a Mesh Asset (Final preferred, else Fast Path) with AABB framing and fixed studio lighting; CPU readback.
- **Content Browser Thumbnails** for Mesh Assets become Mesh Preview Render stills (async queue, visibility-prioritized); Texture Assets keep image thumbnails.
- **Asset Inspector** mode: Content Browser selection of a Mesh Asset shows Inspector with interactive **Mesh Preview** (orbit / zoom / reset) plus read-only identity.
- Retire base-color-as-mesh-thumbnail when a 3D still can be produced; fail soft to Mesh placeholder.
- Skinned meshes: bind-pose (rest) stills only in this slice — no AnimationPlayer playback in thumbnails/preview.
- Docs already grilled: CONTEXT terms + [ADR 0022](../../../docs/adr/0022-mesh-preview-render-offscreen.md).

## Capabilities

### New Capabilities
- `mesh-preview-render`: Shared offscreen Mesh Asset render, thumbnail still generation/cache integration, async visibility-prioritized queue.
- `asset-inspector-mesh-preview`: Content Browser Mesh selection → Asset Inspector shell with interactive Mesh Preview and read-only identity.

### Modified Capabilities
- (none required at main-spec level for this slice; thumbnail behavior is owned by the new capabilities. If a future `content-browser` main spec exists, archive may merge deltas there.)

## Impact

- Code: `ThumbnailGenerator` (replace mesh texture shortcut), new Mesh Preview Render / RT / queue, `RenderSystem` or sibling offscreen path, Slint Inspector Asset mode + preview image, Content Browser selection → Inspector wiring, thumbnail cache invalidation.
- Docs: CONTEXT (Mesh Preview Render / Thumbnail / Mesh Preview / Asset Inspector); ADR 0022.
- Tests: framing helpers, thumbnail generation prefers 3D still over baseColor when render succeeds, selection → Asset Inspector visibility, queue prioritization (unit where feasible).
- Non-goals: HDRI/IBL, Import-settings editing in Asset Inspector, Camera Preview RT reuse, main-viewport blit, AnimationClip/Texture 3D, bare unregistered glTF entries, persisting orbit angles.
