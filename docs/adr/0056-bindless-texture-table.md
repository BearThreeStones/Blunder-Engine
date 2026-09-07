# Bindless texture table is device-wide color textures, not shadow

Mesh color sampled images and samplers live in one resident table per Vulkan device. Draws select them by stable index. The shadow map stays a dedicated comparison binding (PCF / SampleCmp). The device must support descriptor indexing; there is no per-draw color-texture fallback path. A full table uses the fallback index and does not fail process start.

Rejected: putting mesh UBOs or bone palettes in this table; a second table per offscreen target; `VK_EXT_descriptor_buffer`; folding shadow into the color table; replacing PCF so shadow can share that table; repacking unique textures from the draw list every frame; raising the Forward mesh draw cap in this change.
