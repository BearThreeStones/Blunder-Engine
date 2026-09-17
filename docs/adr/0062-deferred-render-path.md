# Deferred Render Path is a hardcoded viewport opt-in sibling

The editor viewport still lights in `ForwardRenderPath` (at most 8 lights per MeshRenderer, one Directional shadow map). We decided v1 is a **Deferred Render Path**: opaque geometry writes a G-buffer, lighting runs in screen space into the existing offscreen color, sharing that offscreen depth. It is a hardcoded sibling of the Forward Render Path, not clustered/tiled lighting, and not a visibility buffer. G-buffer vs lighting stay inside this path as callbacks. Editor Deferred dispatch on the viewport graph is a G-buffer Pass then a Lighting Pass ([ADR 0069](0069-frame-graph-deferred-split.md)), not one Scene wrapping `renderFrame`. Overlay and copy Passes on that graph are [ADR 0068](0068-frame-graph-viewport-overlays.md). Scene wire for Forward: [ADR 0067](0067-frame-graph-viewport-wire.md). The editor viewport opts in with `BLUNDER_EDITOR_DEFERRED` (default off). Camera Preview, Mesh Preview, Scene Thumbnail / Capture, and Player stay on the Forward Render Path. Domain: [CONTEXT.md — Deferred Render Path](../../CONTEXT.md).

**Status:** accepted

## Considered Options

- **Clustered / tiled deferred lighting on forward geometry** — rejected. Grill: this change is a Render Path (G-buffer then lighting), not a new light-culling scheme. The Light evaluation cap stays 8 per receiver. Clustered can sit on the lighting pass later.
- **Visibility buffer** — rejected. Splits materials, Bindless, and skinning in the same change.
- **Wait for Frame graph GPU, then put deferred on the graph** — rejected. Frame graph Execute is still CPU callbacks with no handle→RHI, alloc, barriers, or command recording. Coupling would stall the viewport path behind several infrastructure slices.
- **Replace Forward as the editor default, or migrate Preview / Thumbnail / Player in this change** — rejected. Overlay, SSAO, pick, and those offscreens still contract on the current color+depth Forward path.
- **Global first-8 lights, ignore Light linking** — rejected. Existing glossary and `scene-light-component` forbid a global first-8. G-buffer stores a compact receiver id so lighting applies linking per pixel.
- **Expand OffscreenRenderTarget into a permanent MRT / blit a deferred-only color / HDR then tone-map** — rejected. Lighting writes the viewport offscreen color; G-buffer is extra textures; depth is the existing offscreen depth so SSAO / outline / pick keep their entry.
