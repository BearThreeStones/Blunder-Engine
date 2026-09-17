## Context

The editor Viewport is forward-only today (`engine/shaders/pbr.slang`): a fragment shader loops a flat `GpuSceneLight lights[8]` array for every fragment, and `scene-light-component`'s "Light evaluation cap" requirement enforces "first 8 in stable EntityId order, drop the rest" everywhere, including the Viewport. A separate, currently uncommitted change (`gpu-driven-rendering`, local to the Windows tree, not present on this cloud checkout) is landing a deferred GBuffer under Blunder's GPU-driven / frame-graph work. This design assumes that GBuffer exists as a dependency — depth plus the material attributes a lighting pass needs — and does not modify, extend, or re-propose it. See `proposal.md` - Why for the user-facing motivation.

Three research briefs informed this design (all read-only, no code written from them directly):
- **Unreal 5.7 brief** (`unreal-clustered-deferred-brief.md`): the Forward Light Grid is a 3D grid (screen tiles × exponential-Z slices, defaults 64 px / 32 slices), filled by one GPU compute pass (`LightGridInjectionCS`), consumed by an optional clustered-deferred pixel shader. Per-cell cap defaults to 32 with linked-list culling *off* by default (hard cap, drop). Directional is never gridded. Debug visualization is an editor-gated heatmap pass.
- **Packt Ch. 7 (Vulkan) brief**: implements 2D tiles + 16 linear Z-bins with CPU assignment. Grill explicitly rejected this topology and CPU assignment for this slice; it is cited here only to record what was considered and declined.
- **Unity/Godot brief**: HDRP and Godot both use GPU compute assignment and both keep directional lights out of the spatial structure (independent convergent evidence for that rule). HDRP's clustered path and Godot's bitmask both avoid a hard cap in favor of global/offset-based lists; Unreal's default is a hard per-cell cap. Grill locked the hard-cap approach (64) over the offset+count approach for this slice.

## Goals / Non-Goals

**Goals:**
- Editor Viewport clustered lighting for point + spot lights, stacked on the existing (external) deferred GBuffer.
- GPU compute build of a 3D froxel grid: screen tiles (default 64 px) × exponential-Z slices (default 32), matching Unreal Forward Light Grid topology.
- Fixed per-froxel cap of 64 assigned lights, overflow dropped with a log + stat, no unbounded/linked-list structure.
- An editor-only occupancy heatmap toggle for accepting the clustering behavior visually.

**Non-Goals:**
- Clustered shadows, area/IES lights in the grid, or translucency clustering (all explicitly deferred).
- Copying Unreal's deprecated `r.UseClusteredDeferredShading` toggle or its `Froxel::` visibility/VSM occupancy system (different system, name collision only).
- A new GBuffer or a new forward render path — this stacks on the existing (external) deferred GBuffer only.
- Any change to, or dependency on the completion state of, the separate `gpu-driven-rendering` change.
- 2D-tile + Z-bin topology or CPU light assignment (the Packt-chapter shape) — considered and declined per Grill lock.
- A global offset+count light-index scheme (the HDRP/Godot clustered shape) — considered and declined in favor of a fixed per-froxel cap.

## Decisions

**3D froxel grid over 2D tiles + Z-bins.** Grill locked 3D froxels; the Packt brief's 2D-tile/16-linear-bin shape is explicitly out. A 3D grid gives each froxel its own light list keyed by (tile, slice) rather than needing a separate bitmask-AND-range step per pixel, which is simpler to reason about and to visualize with a heatmap keyed by froxel occupancy.

**Exponential Z slicing, not linear.** Reuses Unreal's `slice = log2(z * B + O) * S` shape (`GetLightGridZParams` / `ComputeZSliceFromDepth` in the Unreal brief) so near-camera detail is denser without changing the flat XY tile size. Both the froxel-fill compute pass and the GBuffer lighting pass's pixel→froxel lookup must use the same exponential mapping and the same near/far bounds derived from the active camera, or a pixel's shading froxel would disagree with the froxel it was assigned to.

**GPU compute fill, not CPU.** All three references converge here except the intentionally-declined Packt chapter: Unreal, HDRP, and Godot's desktop renderer all assign on GPU. The Unity/Godot brief's own recommendation to do CPU assignment for "slice 1" is not taken — Grill locked GPU compute explicitly, and CPU assignment would also reintroduce the frame-graph/render-thread coupling this change stacks away from.

**Fixed per-froxel cap (64) with drop-and-record, not a linked list or global offset+count list.** Unreal supports both (`r.Forward.LightLinkedListCulling` toggles between them); HDRP's clustered path and Godot's bitmask both lean toward unbounded-total/global-index shapes. Grill locked a flat `uint froxelLightIndices[numFroxels * 64]` plus `uint froxelLightCount[numFroxels]` — no atomic allocator, no compaction pass, no prefix sum, and a trivially fixed memory budget (`numFroxels * 64 * 4 bytes`). This is the same shape the Unity/Godot brief's own author recommended for a bounded slice, just at cap 64 instead of 32, matching Grill's lock.

**Cap keeps lights in stable order, not arbitrary GPU write order.** The compute pass iterates candidate point/spot lights in the same stable order used elsewhere for light selection (matching `scene-light-component`'s "stable EntityId order" convention) and keeps the first 64 that overlap a given froxel. Without an explicit stable order, which 64 of >64 overlapping lights survive could vary frame-to-frame with GPU scheduling, causing visible flicker in overflowed froxels.

**Spot lights use their bounding sphere (range) for froxel overlap in this slice; the cone test stays in the shading BRDF.** The paper brief flags that a sphere-only overlap test over-assigns spot lights to froxels their cone doesn't actually reach. Grill scope excludes a true cone-vs-froxel test this slice; the design accepts the resulting conservative over-assignment (a froxel may list a spot light that contributes nothing at some pixels in that froxel) because the existing per-pixel BRDF already applies the cone attenuation, so shaded output is still correct — only froxel occupancy (and hence the heatmap and the 64-cap) is conservatively pessimistic for spots. This is a candidate for a tighter cone-vs-AABB test as a fast-follow, not required for this slice's correctness.

**Directional lights are never in the grid.** Unreal, HDRP, and Godot all exclude directional lights from their spatial structure and instead shade them once per pixel via the existing fullscreen path. This change keeps that split exactly: the froxel grid and its cap only ever hold point/spot lights, and the existing fullscreen deferred directional pass is untouched.

**Narrow the `scene-light-component` "Light evaluation cap" delta to the editor Viewport only.** Player, Camera Preview, Placement Preview, Mesh Preview, and Scene Thumbnail keep the existing flat 8-light forward cap unchanged; only the editor Viewport's new clustered GBuffer lighting path is exempt, bounded instead by the per-froxel cap. This avoids widening scope into any non-Viewport render path.

**Heatmap reuses the froxel-fill output directly and is editor-gated the same way Unreal gates `DebugLightGridPS`.** No separate light-counting pass: the overlay reads `froxelLightCount` per froxel and colors that froxel's screen tile, so what the heatmap shows is exactly what the lighting pass consumed (post-cap), not a separate pre-cap estimate.

## Risks / Trade-offs

- **[Risk]** The deferred GBuffer this change stacks on is uncommitted and local to the Windows tree; its exact depth/view-reconstruction interface may shift before it lands on `main`. → **Mitigation**: this design only assumes the GBuffer exposes scene depth and a view/projection sufficient to reconstruct view-space depth per pixel — the same minimal contract any deferred lighting pass needs. Tasks should confirm that contract against the landed interface before wiring the froxel lookup, rather than assuming today's local shape.
- **[Confirmed 1.1]** The in-progress Viewport deferred GBuffer is sufficient: lighting already reconstructs world position from shared offscreen depth + `invViewProjection` (`deferred_lighting.slang` / `DeferredLightingUniformData`). `ForwardFrameState` already carries `view`, `projection`, `near_clip`, and `far_clip` for the exponential Z mapping. No gap vs this design's assumed interface; no GBuffer format or reconstruction change is required.
- **[Risk]** A very large or camera-near point/spot light can overlap most of the grid, filling many froxels toward their 64-light cap and causing broad overflow drops. → **Mitigation**: this is a known Unreal failure mode too (their far-slice AABB culling is intentionally poor); the occupancy heatmap this change ships is the acceptance tool for spotting it, and tightening large-light culling is an explicit fast-follow, not required for this slice.
- **[Risk]** Conservative spot-as-sphere froxel assignment inflates occupancy for spot-heavy scenes, making the 64-cap bind sooner than a true cone test would. → **Mitigation**: accepted for this slice (see Decisions); visible via the heatmap; a cone-vs-froxel-AABB test is the natural fast-follow if profiling or the heatmap shows it binding in practice.
- **[Risk]** Fixed `numFroxels * 64` index buffer grows with resolution and tile-size choice (e.g., 1080p at 64 px tiles × 32 slices is on the order of a few hundred thousand froxels; at 64 indices each this is tens of MB). → **Mitigation**: defaults are chosen to match Unreal's shipping defaults, which are known-acceptable on desktop; this is a fixed, predictable allocation rather than a dynamic one, so it is a one-time budget check, not a runtime risk.

## Migration Plan

This is a new, additive Viewport-only capability with no persisted data format and no change to scene/asset serialization. It has no effect until the (external) deferred GBuffer it stacks on is present; until then, the editor Viewport continues to use its current forward shading path unchanged, so there is no rollback beyond not enabling/merging this change's Viewport wiring.

## Open Questions

- Exact overflow log message shape and the name/surface of the Viewport "dropped lights" stat (e.g., existing Viewport stats overlay vs. a new counter) are an implementation detail for `tasks.md`, not a spec-level decision.
- Whether froxel tile size (64 px) and Z slice count (32) should become author-configurable (CVar-equivalent) later is left open; this slice ships fixed defaults only, matching what the User stories and specs require.
