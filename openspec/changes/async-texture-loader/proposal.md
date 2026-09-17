## Why

Opening a textured scene still stalls the tick: `AssetManager::loadTexture2D` reads and decodes on the owner thread, then `VulkanImage::uploadPixels` submits a copy and waits on a fence. Grill locked a **Texture Loader** that splits that work: Jobs for file + decode, a Render upload queue for staging and copies, Bindless fallback until the copy is done. Decision: [ADR 0058](../../../docs/adr/0058-async-texture-loader.md); domain: [CONTEXT.md — Texture Loader](../../../CONTEXT.md).

## What Changes

- Add a **Texture Loader** beside AssetManager and the Bindless table: request coalescing, Job-backed CPU read/decode into Job data, a staging-buffer pool, copy command buffers submitted without waiting the tick, fence poll that writes the Bindless slot when the transfer finishes.
- Until that write, mesh draws keep sampling **Bindless fallback index 0** (already the overflow/missing path). No checkerboard product, no hide-until-ready.
- v1 is **color material textures only**. Mesh/index uploads, a dedicated Vulkan transfer queue, GPU mipgen, KTX streaming, ThumbnailGenerator, Pull cook, and the Engine GPU cache stay out.
- Editor and Player (including Headless: CPU Jobs may run; no GPU upload without a device). Cancel in-flight work on scene drop and process quit.

## User stories

1. I open a scene with several large material textures: the first frames show the Bindless fallback, then the real textures appear, and the viewport does not freeze on decode or `vkWaitForFences`.
2. I cause the same texture path to be requested again while it is still in flight: the engine uploads it once.
3. I switch scenes while uploads are still in flight: the process does not crash, and completed copies do not write GPU images that were dropped with the old scene.
4. I quit the Editor or Player while uploads are still in flight: the process exits without hanging on fence wait or Worker join.
5. I start Headless Editor or Headless Player: there is no Vulkan upload when there is no device, and the process still exits cleanly.

## Capabilities

### New Capabilities

- `async-texture-loader`: Texture Loader — Job-backed CPU read/decode, GPU upload queue (staging pool, copy CB, fence poll), Bindless fallback until resident, request coalesce, cancel on scene drop and quit. Textures only.

### Modified Capabilities

- *(none — Bindless fallback index 0 already exists; this change is the caller that delays `writeSlot` until the copy completes. Job System isolation already forbids RHI inside a Job.)*

## Impact

- **Engine:** Texture Loader under `engine/src/runtime/function/render/` (GPU half) plus a small CPU request path that Submits Jobs; `ensureUploadedTexture` / `uploadPixels` stop blocking the tick for material textures; Bindless `writeSlot` only after fence.
- **Tests:** first-party test for request coalesce, cancel, and CPU Job decode into buffers (no Vulkan if possible); Headless stories 4–5 on existing Headless launch.
- **Docs:** CONTEXT Texture Loader; [ADR 0058](../../../docs/adr/0058-async-texture-loader.md).
- **Non-goals:** second AssetManager; GPU copies inside a Job; dedicated transfer queue family; mesh buffer uploads; Thumbnail / cook / Slang cache; C-ABI texture schedule.
