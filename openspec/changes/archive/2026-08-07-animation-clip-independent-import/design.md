## Context

ADR 0028 and CONTEXT.md already define AnimationClip as independent of Mesh. Runtime Import still copies companions under `Models/{host}/companions/` (or `_standalone_companions/`), writes `companion_animation_sources` on Mesh YAML, and Mesh Reimport re-extracts those companions — contradicting the glossary and confusing Task 1 / DogWalk workflows.

Related prior changes: `companion-animation-gltf-import`, `standalone-animation-import`, `content-browser-asset-delete`. This change implements ADR 0028 packaging rules without inventing a separate Skeleton Asset.

## Goals / Non-Goals

**Goals:**
- Clip Intermediate + descriptor `source` under `Resources/Animations/<stem>/`
- Remove Mesh packaging field and host-folder companion layout
- Gestures only: multi-select / near-disk → independent Assets + optional bone warn
- Independent Mesh vs Clip Reimport
- One-shot migration of legacy Intermediate layouts
- Tests that lock ADR 0028 behavior

**Non-Goals:**
- Separate Skeleton Asset (Unreal-like) in this change
- Auto-filling AnimationPlayer name→GUID maps at Import
- Cascade-delete UX dialogs beyond current delete service
- Rewriting COLLADA-era `openspec/specs/asset-import` historical scenarios except via ADDED deltas
- Content Browser folder UX redesign beyond path outcomes

## Decisions

1. **Intermediate root = `Resources/Animations/<stem>/`**  
   Holds companion exchange glTF/GLB (and aligns with existing clip YAML layout). Rejected: keep `Models/{mesh}/companions/` (Mesh-child teaching); keep `_standalone_companions` as a second product path.

2. **No `companion_animation_sources`**  
   Mesh YAML drops the field (ignore on load if present until migration clears it). Clip descriptor `source` is authoritative for Reimport. Rejected: optional role-pack list on Mesh (still a fake dependency).

3. **Batch semantics**  
   Host + companions in one batch: Import each as its Asset type; if exactly one skinned host is present, warn on bone mismatch vs that host; never persist packaging links; never invent Mesh for companion-only batches (standalone path stays). Near-disk discovery registers independent Clips the same way (no write-back to Mesh).

4. **Reimport**  
   `Reimport(Mesh)` → mesh Intermediate only. `Reimport(Clip)` → re-extract from Clip `source`. Rejected: Mesh Reimport cascading companion clips.

5. **Migration**  
   On project open or first Import/Reimport touching legacy paths: move `Models/**/companions/*` and `Models/_standalone_companions/*` into `Resources/Animations/<stem>/`, rewrite Clip `source` / clear Mesh field, preserve Clip GUIDs. Prefer a dedicated migrator callable from Import init over silent per-file only.

6. **Dependency graph**  
   Do not add Mesh→Clip edges. Scene/AnimationPlayer references remain the consumer edges.

## Risks / Trade-offs

- [Old Test Project Intermediate] → Mitigation: migration + manual checklist to re-Import Chocomel after upgrade  
- [Tests asserting `companion_animation_sources` / `companions/`] → Mitigation: rewrite `asset_import_test` in same change  
- [Authors expecting Mesh Reimport to refresh clips] → Mitigation: docs + log when Mesh Reimport no longer touches clips  
- [Bone warn needs a host in batch] → Mitigation: warn only when a skinned host is present; companion-only batches skip bone compare  

## Migration Plan

1. Ship migrator + new Import paths behind same editor build.  
2. On load/Import: migrate legacy folders; strip `companion_animation_sources`.  
3. Rollback: restore previous editor binary; migrated files remain under `Animations/` (forward-compatible enough for Clip `source`).  

## Open Questions

- Exact migration trigger (every Content Browser refresh vs once per project via `.blunder` stamp) — prefer once-per-project stamp.  
- Whether embedded animations inside the mesh glTF still extract to Clip Assets under `Animations/` (yes; unchanged product: Mesh + N Clips, but Clips remain independent Assets).
