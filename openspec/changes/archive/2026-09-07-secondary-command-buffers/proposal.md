## Why

Viewport, Camera Preview, Mesh Preview, shadow, overlay, and SSAO still record draws inline on one PRIMARY command buffer. Multi-thread recording cannot start until each of those render passes can execute SECONDARY buffers with inheritance. Jobs must not call RHI, so this change proves that API on the owner thread first.

## What Changes

- Add a device-owned **Secondary command buffer pool**: SECONDARY level, one command pool on the owner thread, slots per frames-in-flight and per pass (plus a Camera Preview bank and an Immediate bank so those streams do not reuse a buffer already executed in the same PRIMARY).
- Record **shadow**, **forward** (opaque / scene overlays / transparent), **outline**, **overlay lines**, **overlay AA**, **SSAO**, and **screen overlays** into those secondaries. The PRIMARY begins each of those passes with `SECONDARY_COMMAND_BUFFERS` and `vkCmdExecuteCommands`.
- Headless with no device allocates nothing. Shutdown waits the in-flight PRIMARY fence (or device idle) before resetting or destroying secondaries.
- Unchanged visuals, pass order, Bindless, Texture Loader copies, and GPU pick. Immediate upload CBs stay PRIMARY.

## User stories

1. I open a scene with materials, shadows, SSAO, and editor overlays (grid, outline, gizmos): it looks like today, and the viewport still presents.
2. I orbit the camera: there is no extra hitch; Bindless fallback then real textures still work; shadows, SSAO, and overlays stay stable.
3. I use Camera Preview and Mesh Preview: both still draw (same forward path, no authorship overlays in those views).
4. I quit the Editor or Player while a frame is still in flight: the process exits and does not hang resetting secondaries.
5. I start Headless Editor or Headless Player with no Vulkan device: no secondary buffers are allocated, and the process still starts and exits.

## Capabilities

### New Capabilities

- `secondary-command-buffers`: Owner-thread Vulkan SECONDARY command buffers for shadow, forward (opaque / scene overlay / transparent), outline, overlay lines, overlay AA, SSAO, and screen overlays; separate Camera Preview and Immediate banks; no device means no alloc; pick and upload stay PRIMARY.

### Modified Capabilities

- *(none — overlay visibility, Camera Preview chrome, Headless host composition, Bindless, and Texture Loader requirements do not change.)*

## Impact

- **Engine:** new pool beside `VulkanContext` immediate PRIMARY pool; `ForwardRenderPath`, `ShadowMapTarget`, overlay passes, and `SsaoPass` begin those passes with secondary contents; RHI `beginRenderPass` gains a contents flag (D3D12 stays inline-only).
- **Tests:** first-party test that no-device initialize allocates nothing, and that Viewport / Camera Preview / Immediate streams do not share an in-flight slot. Windowed stories 1–4 stay on the checklist.
- **Docs:** CONTEXT glossary; [ADR 0059](../../../docs/adr/0059-secondary-command-buffers.md) (written in apply).
- **Non-goals:** Job or worker-thread recording; D3D12 bundles; GPU pick / hybrid pick; converting Texture Loader copies; dedicated transfer queue; changing pass order or shading.
