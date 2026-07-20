## Context

`asset-pipeline-pull` implemented Pull + GUID + Source Export with **glTF Intermediate**. Grill + ADR 0013 flip mesh Intermediate to **COLLADA** while Final stays `.blunder/cooked` binaries. Docs (`CONTEXT.md`, `CONTENT_LAYOUT.md`, ADR 0012/0013) already reflect the domain; this change updates runtime Import/Cook/Fast Path/tests and OpenSpec deltas.

## Goals / Non-Goals

**Goals:**
- Single mesh Intermediate body format: COLLADA (`.dae`)
- Source Export: FBX/OBJ/glTF/GLB → `.dae` + Source archive dual-write
- `.dae` + images: Intermediate-direct Import
- Lazy Intermediate Upgrade for legacy glTF Intermediate (GUID stable; fail-soft)
- Fast Path / Cook read COLLADA via Assimp; `texture_guids` remains Mesh→Texture authority

**Non-Goals:**
- Final = glTF or Collada2gltf shipping product
- Texture Assets stored as `.dae`
- Blender CLI / `.blend` auto-export
- Rewriting GUID into COLLADA `<extra>`

## Decisions

1. **Assimp for COLLADA write and read** — Source Export uses Assimp Collada exporter; Fast Path and mesh Cook load Intermediate `.dae` via Assimp (same stack as export). Prefer one mesh Intermediate reader over keeping cgltf as primary for new Assets.

2. **cgltf only for legacy Fast Path** — While descriptor `source` still points at glTF/GLB (upgrade failed or not yet run), Fast Path/Cook MAY use the existing glTF path. After successful upgrade, cgltf is not required for that Asset.

3. **Extension routing** — Intermediate-direct: `.dae`, images. Source Export whitelist: `.fbx`, `.obj`, `.gltf`, `.glb`. `.blend` remains rejected.

4. **Intermediate Upgrade hook** — On registry scan / project open: for each mesh Asset whose Intermediate `source` ends with glTF/GLB, Assimp-convert to sibling `.dae`, rewrite `source`, archive former file under Source if `archived_source` empty, invalidate Finals. Failure: no partial `.dae` as `source`; log; leave files as-is.

5. **texture_guids authority** — Unchanged: graph and invalidation use descriptor `texture_guids`. After Export/Upgrade, best-effort refresh of `texture_guids` from materials when Texture Assets can be registered; COLLADA image URIs alone do not create graph edges.

6. **Docs** — Prefer keeping grill docs; only touch if code paths diverge from ADR 0013.

## Risks / Trade-offs

- **[Risk] Assimp Collada round-trip loses data → Mitigation:** keep archived Source; Reimport from Source; tests on OBJ/glTF→dae→load vertex counts
- **[Risk] Dual readers (Assimp + cgltf) during migration → Mitigation:** upgrade is eager on scan; cgltf path gated on legacy `source` extension only
- **[Risk] Existing tests/fixtures assume `.gltf` Intermediate → Mitigation:** update fixtures and smoke to `.dae`; keep one legacy-upgrade test with glTF `source`
- **[Risk] Sibling change `asset-pipeline-pull` not archived → Mitigation:** deltas target the same capability names; archive order: pull first or archive both with care

## Migration Plan

1. Land Import/Export + Cook/Fast Path COLLADA support behind updated extension tables
2. Enable Intermediate Upgrade on scan
3. Update tests and smoke
4. Existing projects: open once → upgrade; failures remain loadable via legacy glTF until fixed
5. Rollback: revert change; glTF Intermediate Assets still on disk for un-upgraded projects; upgraded Assets need Reimport from archived Source to restore glTF Intermediate if ever required (not supported as a product path)

## Open Questions

- None blocking apply; Assimp Collada export flags can be tuned during implementation if fixtures fail.
