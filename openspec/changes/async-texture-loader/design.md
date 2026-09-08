## Context

See proposal.md for why. Grill locked texture-only v1, Jobs for file + decode, Bindless fallback index 0 until resident ([ADR 0058](../../../docs/adr/0058-async-texture-loader.md)). Today `AssetManager::loadTexture2D` decodes on the tick and `VulkanImage::uploadPixels` creates a staging buffer, copies, then `endImmediateCommands` waits the graphics-queue fence. Bindless already uses slot 0 as fallback ([ADR 0056](../../../docs/adr/0056-bindless-texture-table.md)). Jobs must not call RHI ([ADR 0057](../../../docs/adr/0057-task-job-system.md)). `submitImmediateCommandsNoWait` already exists. ThumbnailGenerator, Pull cook, and Engine GPU cache stay separate queues.

## Goals / Non-Goals

**Goals:**

- One Texture Loader type: request by virtual path, coalesce, cancel, CPU Jobs, GPU upload queue, Bindless write after fence.
- Tick presents with fallback while work is in flight (no Job barrier and no `vkWaitForFences` on that path).
- Tests cover coalesce / cancel / CPU Job decode without requiring a windowed editor.

**Non-Goals:**

- A second AssetManager or changing Pull cook.
- Dedicated Vulkan transfer queue family or queue-family ownership barriers.
- Mesh/index buffer streaming, GPU mipgen, KTX/texbin streaming, dropping `Texture2DAsset` CPU pixels in v1 (AssetManager still owns Loaded pixels).
- Replacing ThumbnailGenerator or the Engine GPU cache.
- Per-Job handles or a poll API on JobSystem; the Loader polls its own Job data.

## Decisions

1. **Texture Loader lives under `function/render/` and is owned by RenderSystem**  
   GPU images, staging, and Bindless writes are RHI. AssetManager stays the CPU cache (`Texture2DAsset`). Headless with no RenderSystem / no device skips GPU upload.  
   *Alternatives:* put it on AssetManager (pulls RHI into resource); a new Context System (duplicates Render lifetime).

2. **CPU half Submits Jobs and does not `wait()` on the tick**  
   Job data is a caller-owned decode slot (path, rgba bytes, size, done/failed flags). Workers run the Job. The tick polls those flags and enqueues GPU work. `wait()` is for shutdown or a scene-flush drain, not for presenting.  
   *Alternatives:* `wait()` per texture (still hitch); a second IO thread pool (Complexity penalty; Job System exists).

3. **GPU half uses the graphics queue and fence poll**  
   Record copy on the RHI/tick owner, `submitImmediateCommandsNoWait` (or equivalent), poll fences like viewport readback. Recycle staging only after the fence.  
   *Alternatives:* dedicated transfer family (extra ownership barriers, not needed to stop the hitch); keep `endImmediateCommands` wait (status quo).

4. **Staging is a pool, not per-upload create/destroy**  
   Rent a CPU-visible buffer for the copy; return it when the fence signals. Cap outstanding staging bytes so one scene open cannot unbounded-allocate.  
   *Alternatives:* one-shot staging as today (alloc churn); persistently mapped ring with no cap (can OOM).

5. **Bindless `writeSlot` only after the copy fence**  
   In-flight paths keep the fallback index. No checkerboard texture and no skip-draw. Overflow still uses fallback (existing table rule).  
   *Alternatives:* hide the mesh until resident (flicker / holes); a second placeholder image.

6. **Coalesce by canonical virtual path; cancel by generation**  
   One in-flight record per path. Scene drop bumps a generation (or request id); completions with a stale generation free staging and do not `writeSlot`. In-flight Jobs always run to completion (no Job cancel); the owner drops their Job data.  
   *Alternatives:* cancel the OS thread (Jobs cannot yield); block scene switch until uploads drain (hurts story 3).

7. **Tests construct the CPU request path with `JobSystem` directly**  
   Coalesce, cancel, and decode-into-buffer do not need Vulkan. GPU submit/fence is covered where a device exists, or by not calling GPU upload in Headless tests.  
   *Alternatives:* only windowed Human stories (too late to catch coalesce bugs).

## Risks / Trade-offs

- [Tick Submits many decode Jobs without a barrier] → Outstanding work drains on Workers; shutdown `wait()` still joins. Do not Submit from a second thread.
- [Job finishes after scene drop] → Generation check before `writeSlot`; leak only the dropped pixel buffer, not GPU images.
- [Staging cap delays visible textures] → Fallback stays on screen; later frames upload. Prefer this over hitching.
- [AssetManager sync `loadTexture2D` still exists] → Loader may call it only from a Job (file + decode into Job data), not from the tick. Do not leave a tick-path `ensureUploadedTexture` that still `endImmediateCommands`-waits for materials.

## Migration Plan

1. Land Texture Loader + tests; wire material texture residency through it; keep mesh buffer upload unchanged.
2. No content or cook-format migration.
3. Rollback: revert Loader and restore blocking `uploadPixels` for materials.

## Open Questions

None.
