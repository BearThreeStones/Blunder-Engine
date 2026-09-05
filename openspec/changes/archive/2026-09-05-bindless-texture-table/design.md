## Context

See proposal.md for why. Grill locked: color-texture bindless only; one resident table per device shared by viewport / Mesh Preview / Camera Preview / Scene Thumbnail / Capture / in-process Player; shadow map stays a dedicated SampleCmp binding; full table uses fallback (not FATAL); no per-draw texture-set fallback; no descriptor buffers; no GPU-driven constants; Forward mesh draw cap stays 256. ADR 0054 covers Slang extract. ADR 0055 covers Engine GPU cache. ADR 0056 covers this table.

Today `ForwardRenderPath` writes per-draw sampled-image/sampler descriptors for base color, metallic-roughness, normal, and occlusion (`pbr.slang` bindings 1–2 and 5–10) plus shadow at 3–4 and UBO at 0. Device create already requires `shaderDrawParameters`. Overlay / SSAO / pick keep hand-written layouts.

Change `shader-resource-layout` is still the exact-match FATAL contract: extracted bindings must equal what the record path writes. This change rewrites what that path writes for color textures; it must not weaken the FATAL.

## Goals / Non-Goals

**Goals:**

- One resident Bindless texture table on `VulkanContext`.
- Mesh color textures sampled by stable index; per-draw set holds UBO, optional bone palette, and shadow comparison bindings.
- Device create requires descriptor indexing; missing support fails like missing `shaderDrawParameters`.
- Host test for index stability and overflow fallback; existing layout test expected-sets updated.

**Non-Goals:**

- Putting mesh UBOs or bone palettes in the table.
- A runtime fallback that keeps per-draw color-texture sets.
- `VK_EXT_descriptor_buffer`, GPU-driven rendering, raising 256.
- Moving overlay / SSAO / pick onto the table.
- Folding shadow PCF into the color table.

## Decisions

1. **Table lives on `VulkanContext`**  
   Create layout, pool, and one descriptor set after device create. Mesh passes bind that set in addition to the per-draw set. Mesh Preview / Camera Preview / Thumbnail already share this context. Player in another process has its own context and its own table.  
   *Alternatives:* one table per offscreen target (Grill rejected); viewport-only bindless with preview still per-draw (dual path).

2. **Color textures leave set 0; shadow does not**  
   `pbr.slang` / `pbr_skinned.slang`: set 0 keeps UBO, shadow sampled image, shadow comparison sampler, and skinned bone palette. Set 1 is two unbounded arrays (`Texture2D` + `SamplerState`) with `NonUniformResourceIndex`. Record path writes four `uint` indices into the existing per-draw UBO. Exact-match FATAL expected set becomes that compact set 0 plus the two set-1 array bindings (descriptor count 1 in the extracted layout, not the table capacity).  
   *Alternatives:* keep today’s 1–10 numbers and only change update style (not bindless); put arrays in set 0 (fights UPDATE_AFTER_BIND flags with UBO).

3. **Capacity 1024, index 0 is fallback**  
   `descriptorCount` on each array binding is 1024. Slot 0 is the existing fallback texture/sampler and is never reused for a loaded material. First free slot ≥ 1 on load; index stays until that GPU texture is destroyed. Table full → return 0, log, do not FATAL. Do not compact on unload (would break other draws’ indices).  
   *Alternatives:* 256 (too close to draw cap, collides unique-texture vs draw limits); grow the set at runtime (Vulkan layout is fixed); FATAL on full (Grill rejected).

4. **Descriptor indexing is required at device pick**  
   Enable `runtimeDescriptorArray`, `shaderSampledImageArrayNonUniformIndexing`, `descriptorBindingSampledImageUpdateAfterBind`, `descriptorBindingPartiallyBound`, and `descriptorBindingUpdateUnusedWhilePending`. Vulkan has no sampler-array counterparts; sampled-image indexing covers the separate sampler array. Fail device init if unsupported. `PARTIALLY_BOUND` applies to unused table slots, not to “shader declared a color texture the record path skipped.”  
   *Alternatives:* keep per-draw sets on old GPUs (Grill rejected); descriptor buffers (Grill rejected).

5. **Bind the table set once per mesh pass**  
   Opaque / transparent / skinned mesh recording binds set 1 for the table, then per draw binds only set 0 (UBO / bones / shadow). Do not `vkUpdateDescriptorSets` color images per draw. Shadow writes stay on the per-draw set.  
   *Alternatives:* repack unique textures from the draw list every frame (Grill rejected); per-draw still occupy four table slots (not bindless).

6. **Engine GPU cache follows source hash**  
   Shader source edits miss bytecode naturally. Do not bump `k_engine_gpu_cache_generation` unless extract/session identity changes without touching `.slang` bytes.  
   *Alternatives:* always bump generation (wipes every machine’s cache for an unrelated reason).

## Risks / Trade-offs

- [Slang extract sees array bindings with a large descriptor count] → Treat the bindless arrays as two layout bindings; do not expect 1024 unique (set, binding) keys. FATAL still compares binding numbers and kinds, not table occupancy.
- [NonUniform index on a wrong slot] → Slot 0 is always a valid fallback image. Overflow and missing textures both use 0.
- [Unbind/reload of a texture mid-session] → Free that index; do not shuffle others. Draws that still hold a stale index until the next UBO write can sample fallback — same class of lifetime as today’s descriptor writes.
- [Validation layers want update-after-bind pool flags] → Create the table pool with `UPDATE_AFTER_BIND` bit; per-draw pool stays as today.

## Migration Plan

1. Ship table + shader rewrite together; first boot misses bytecode because `pbr.slang` bytes change.
2. Rollback: revert the change; leftover table code is unused. No content migration.

## Open Questions

None.
