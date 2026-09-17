# GPU-driven rendering is the default for static opaque geometry

Static opaque and alpha-clip MeshRenderers submit through GPU Meshlet culling and indirect draws (task/mesh shaders when `VK_EXT_mesh_shader` exists, otherwise compute plus vertex/fragment). Shading stays on the existing Forward or Deferred Render Path. Directional shadows use the same geometry path. Skinned, blend-transparent, pick, outline, and Preview/Thumbnail stay on the CPU draw list. Occlusion is previous-frame Hi-Z plus an early/late pair, not a depth prepass. Domain: [CONTEXT.md — GPU-driven rendering](../../CONTEXT.md).

**Status:** accepted

## Considered Options

- **Per-scene opt-in (Sponza only)** — rejected. Grill. Unreal GPU Scene and Unity GPU Resident Drawer are default or project-level, not a demo-scene flag.
- **Replace every mesh surface, including skinned and Preview** — rejected. Grill. Those consumers still contract on the CPU draw list.
- **Visibility buffer / Nanite-style resolve** — rejected. Grill. This change upgrades geometry submission; it does not replace G-buffer or Forward lighting.
- **Depth prepass then Hi-Z** — rejected. Grill. The book’s previous-frame pyramid plus late pass avoids an extra geometry pass on a path that has no depth prepass today.
- **Require mesh shaders or skip GPU-driven** — rejected. Grill. Compute plus VS/FS indirect is the book’s non-mesh path.
