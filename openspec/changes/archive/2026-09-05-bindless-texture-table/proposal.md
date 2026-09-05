## Why

Forward mesh draws still bind a per-draw descriptor set that copies every material sampled image and sampler. Unique textures therefore cost descriptor writes every draw, and the Shader resource layout still lists those textures as sequential set-0 bindings. This is the third of three grilled render fills (layout → Engine GPU cache → Bindless texture table): put color material textures in one device-wide table and let draws select them by stable index.

## What Changes

- Add a **Bindless texture table**: one resident sampled-image + sampler table per Vulkan device, shared by every mesh shading path on that device (editor viewport, Mesh Preview, Camera Preview, Scene Thumbnail / Capture, Player in that process).
- Mesh draws select color material textures by stable index (valid while that GPU texture remains loaded). Per-draw constant buffers stay a normal descriptor set. The shadow map stays a dedicated comparison binding and is **not** an entry in this table.
- A full table does not fail process start; extra unique textures use the fallback index and log. Overlay, SSAO, and pick do not use this table.
- Device must support descriptor indexing (same class of requirement as `shaderDrawParameters` today). No per-draw texture-set fallback path.
- Unchanged: exact-match FATAL between Shader resource layout and record-path writes; Engine GPU cache; Forward mesh draw cap (256); no GPU-driven rendering; no `VK_EXT_descriptor_buffer`.

## User stories

1. In the editor viewport, several meshes with several material textures look the same as before bindless; shadows stay the current PCF and are not in that table.
2. In the same session, Mesh Preview and Camera Preview mesh textures stay correct (they share this device’s one table).
3. When unique material textures exceed table capacity, extras use the fallback texture; the editor does not crash.
4. A frame still records at most 256 mesh draws; overflow is truncated the same as today, not because bindless grew.

## Capabilities

### New Capabilities

- `bindless-texture-table`: Device-wide resident sampled-image/sampler table for mesh color textures; draws select by stable index; shadow stays a comparison binding; full table uses fallback; overlay/SSAO/pick out of scope.

### Modified Capabilities

- *(none — Shader resource layout exact-match FATAL stays; Engine GPU cache miss rules stay; Play still a separate process with its own table on its device.)*

## Impact

- **Engine:** `VulkanContext` descriptor-indexing features; a table owned with the device; `pbr.slang` / `pbr_skinned.slang` sample color textures by index; `ForwardRenderPath` and mesh-preview record path write indices instead of per-draw texture descriptors; shadow writes stay dedicated.
- **Hosts:** Editor and Player each have one table per their Vulkan device. Same-process previews share the editor table.
- **Docs:** `CONTEXT.md` Bindless texture table / Forward mesh draw cap (already grilled); [ADR 0056](../../../docs/adr/0056-bindless-texture-table.md).
- **Non-goals:** Mesh UBO / bone palette in the table, a fallback that keeps per-draw texture sets, descriptor buffers, folding SampleCmp shadow into the table, replacing PCF, repacking the table from the draw list every frame, a second table per offscreen target, raising the Forward mesh draw cap.
