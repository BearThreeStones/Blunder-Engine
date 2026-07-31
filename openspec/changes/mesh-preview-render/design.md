## Context

`ThumbnailGenerator::generateMeshThumbnail` loads the Mesh and, if a material has a base-color texture, **resizes that texture** as the Content Browser thumbnail — so Mesh Assets often look like texture atlases (e.g. Sponza foliage). There is no Mesh Asset offscreen render path. **Camera Preview** (ADR 0018) already uses a dedicated secondary offscreen + readback for scene **Camera Components**; Mesh Asset preview must not share that RT. Grilling locked product terms in CONTEXT and [ADR 0022](../../../docs/adr/0022-mesh-preview-render-offscreen.md).

## Goals / Non-Goals

**Goals:**
- Shared **Mesh Preview Render** (Final preferred, else Fast Path) with AABB framing + fixed studio lighting into a **dedicated** offscreen RT + CPU readback.
- Mesh **Content Browser Thumbnails** from that render (async, visibility-prioritized); invalidate/replace old texture-based Mesh caches when regenerating.
- **Asset Inspector**: Browser selects Mesh Asset → Inspector shows interactive Mesh Preview (orbit/zoom/reset) + read-only identity.
- Skinned Mesh: bind-pose stills only in this slice.

**Non-Goals:**
- HDRI/IBL studio lighting.
- Import/Reimport settings editing in Asset Inspector.
- Reusing Camera Preview RT or main viewport offscreen.
- AnimationPlayer playback in thumbnails/preview.
- 3D preview for Texture / AnimationClip / bare unregistered glTF.
- Persisting orbit angles to disk.

## Decisions

1. **Dedicated Mesh Preview offscreen RT** — Same pattern as Camera Preview, separate owner/lifecycle. Rejected: share Camera Preview RT; blit into main viewport (ADR 0018 precedent).
2. **One Mesh Preview Render service** — Thumbnail = one-shot capture to existing thumbnail cache; Inspector = live/on-dirty frames into Slint `image`. Rejected: two unrelated render stacks.
3. **Pull path: Final then Fast Path** — Matches viewport authorship. Rejected: Final-only (blocks uncooked); Intermediate-only (diverges from cooked look).
4. **Async + visible-first thumbnail queue** — Selected Asset Inspector render is a high-priority slot. Rejected: sync-on-refresh (UI stalls on large meshes).
5. **Studio lighting + auto AABB frame; draw all submeshes** — Stable product stills. Rejected: unlit-only; scene-light coupling.
6. **Minimal Asset Inspector** — Preview + read-only identity. Rejected: Import UX and dependency browser in this slice.
7. **Browser selection drives Asset mode** — Not MeshRenderer Entity selection as the primary path.

## Risks / Trade-offs

- [GPU cost for many Mesh thumbnails] → Mitigation: async queue, visibility priority, thumbnail-size RT, shadows off or minimal.
- [Inspector vs Browser selection conflict] → Mitigation: explicit selection ownership rules (Browser Mesh selection enters Asset mode; Entity selection restores Entity Inspector).
- [Large multi-material meshes (Sponza) first frame latency] → Mitigation: placeholder until queue completes; cache hit thereafter.
- [Skinned bind-pose may look “T-pose ugly”] → Accept for v1; clip scrub is a later slice.
- [Readback stalls] → Mitigation: reuse Camera Preview-style async readback patterns where possible; never block SDL event pump on whole-folder sync generate.

## Migration Plan

- No Project content migration. On first regenerate after upgrade, Mesh thumbnail cache entries re-key/invalidate when source/descriptor mtime changes (existing cache rules) so texture-based PNGs are replaced by 3D stills.
- Rollback: revert change; ThumbnailGenerator can restore base-color shortcut.

## Open Questions

- Exact RT pixel size for thumbnails vs Inspector preview (e.g. 128 vs ≤480 long edge) — implementer picks; keep Thumbnail ≤ current thumbnail_size unless quality demands a 2× supersample then downscale.
- Whether Asset Inspector clears when clicking empty Browser chrome vs only on Entity selection — prefer: Entity selection exits Asset mode; folder-only selection may clear preview.
