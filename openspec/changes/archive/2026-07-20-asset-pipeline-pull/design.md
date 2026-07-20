## Context

Grill session (Rémi Arnaud *Game Asset Pipeline* + Unreal/Godot comparison) locked the domain model in `CONTEXT.md`. Today: YAML mesh/texture descriptors with GUID + registry; cooked `.meshbin`/`.texbin`; load prefers cooked then falls back to glTF; Import copies into `Resources/` and writes descriptors; `cookIfStale` at startup; efsw watches only `Assets/` for Content Browser refresh; scenes use path `mesh` fields without GUID.

## Goals / Non-Goals

**Goals:**
- Pull daily loop: resolve Asset by GUID, Fast Path Intermediate when Final stale/missing, on-demand Cook, watch-driven invalidation
- Stable Asset References (GUID) including Scene→Mesh
- Minimal dependency graph for invalidation propagation
- Import + Source Export (FBX/OBJ→glTF) with Source archive dual-write and Reimport

**Non-Goals:**
- `.blend` / `.psd` automatic export; Blender CLI as a hard dependency
- Remote Control (open DCC from editor)
- Material Asset nodes / full packaging Manifest cull
- Renaming on-disk roots (`Assets/` / `Resources/`)

## Decisions

1. **Three-tier on existing roots** — Keep `Assets/`, `Resources/`, `Resources/Source/`, `.blunder/cooked/`. Descriptors live only under Assets; Intermediate bodies under Resources (non-Source); Source archive under Source root; Final under cooked cache.

2. **GUID is canonical identity** — Registry maps GUID↔descriptor path. Runtime load APIs gain GUID entry (path remains for migration and display). Scene JSON stores mesh as GUID Asset Reference; on load, legacy path fields resolve via `findGuidForPath` and rewrite on save.

3. **Scene is an Asset** — Add `guid` to `.scene.asset`; register on create/open/scan; Scene participates in the dependency graph as a root.

4. **Pull + Fast Path** — `AssetCompilerService` (or successor) exposes cook-one / cook-stale-dependents. `AssetManager` load: fresh Final → use; else Intermediate Fast Path + request Cook; when Cook completes, prefer Final on next load (optional live replace later). Startup full `cookIfStale` may remain as optional warm-up, not the definition of freshness.

5. **Minimal dependency graph** — Edges: Scene→Mesh Asset; Mesh→Texture Asset when descriptor/import records an explicit Texture GUID; Asset→descriptor + Intermediate `source` file as freshness leaves. No Material nodes in v1. Graph rebuilt from descriptors + scene documents (and registry), not hand lists.

6. **Asset Watch** — Watch Assets root and Intermediate Resources tree. Descriptor/Intermediate change → mark Final stale + dependents. Source root change → automatic Reimport for Assets that archive that path.

7. **Source Export via Assimp whitelist** — FBX/OBJ → glTF Intermediate under Resources; original copied to Source root; descriptor records Intermediate `source` plus optional archived Source path. Direct glTF/PNG Import unchanged (Intermediate only, no Source archive required).

8. **Assimp role** — Use in-tree Assimp for Source Export write; keep cgltf as primary glTF runtime reader for Fast Path / cook input. Do not replace the whole mesh stack with Assimp in v1.

## Risks / Trade-offs

- **[Risk] GUID migration breaks old scenes → Mitigation:** resolve path→GUID on load; save writes GUID; tests for both shapes
- **[Risk] Watching all of Resources is noisy → Mitigation:** debounce; ignore Source subtree for Intermediate invalidation; ignore cooked; suppress self-writes during Import/Cook
- **[Risk] Assimp FBX quality variance → Mitigation:** whitelist only; document known limits; keep Intermediate glTF as editable checkpoint
- **[Risk] Auto-Reimport on Source thrash → Mitigation:** debounce; coalesce; never block UI without progress
- **[Risk] Fast Path vs Final divergence → Mitigation:** same import settings applied at Cook; meta tracks descriptor+source mtimes; document that Final wins when fresh

## Migration Plan

1. Domain docs: `CONTEXT.md` (done), ADR, update `CONTENT_LAYOUT.md`
2. Identity: scene `guid` + GUID mesh refs + registry scan for scenes
3. Pull cook + Fast Path formalization + watch invalidation
4. Dependency graph + propagation tests
5. Source Export + dual-write + Reimport + Source watch
6. Rollback: path mesh fields and cooked fallback remain readable until migration complete

## Open Questions

- Live replace of Loaded Assets when Cook finishes in the same session (nice-to-have vs next-load-only) — default **next load / explicit reload** for v1 unless cheap
- Exact descriptor field name for archived Source (`source_asset` vs `archived_source`) — pick one in implementation and document in `CONTENT_LAYOUT.md`
