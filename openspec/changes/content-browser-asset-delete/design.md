## Context

Content Browser can Import, Refresh, and open/select Mesh descriptors, but has no product delete. `AssetRegistry::unregisterGuid` exists without callers. Authors recovering from a bad Import (e.g. Mesh without companions) must hand-edit `Assets/` and `Resources/` and hope the registry matches.

## Goals / Non-Goals

**Goals:**
- Delete a selected Asset from Content Browser with coordinated descriptor + registry + Final cleanup.
- Best-effort Intermediate `source` file removal.
- Refuse when `dependentsOf(guid)` is non-empty (v1).
- Work for Mesh, Texture, and AnimationClip descriptors (AnimationClip has no Final cook today).

**Non-Goals:**
- Cascade delete of dependents or outbound Texture/clip Assets.
- Undo / History for delete.
- Folder delete, multi-select delete batch UI polish beyond one selected asset.
- Deleting Source archives under `Resources/Source/`.
- Auto-rewriting Scene YAML soft references.

## Decisions

1. **UI** — Highlight selected grid item; Delete key and context-menu "Delete" invoke the same callback when Content Browser has focus and a descriptor is selected (not a folder).
2. **Service** — `AssetDeleteService` (or equivalent) owns the sequence: resolve GUID → check dependents → delete Intermediate `source` (and Mesh `companion_animation_sources` Intermediate bodies only, not clip Assets) → delete descriptor → `unregisterGuid` → `markFinalStale` for Mesh/Texture → rebuild dependency graph → Content Browser refresh.
3. **Dependents gate** — If any Scene (or other Asset) depends on the GUID, refuse and surface a clear message. Authors must detach references first (v1).
4. **Companion clips** — Deleting a Mesh does **not** delete AnimationClip Assets created from companions; clips remain independent GUID Assets.
5. **Partial failure** — Prefer: if descriptor delete + unregister succeed, still refresh; log Intermediate/Final cleanup failures without leaving the GUID registered.

## Risks / Trade-offs

- Refuse-on-dependents may feel strict until Scene detach UX exists — acceptable for v1 safety.
- Leaving orphan Intermediate companions under `Models/{mesh}/companions/` if we only delete listed paths carefully — prefer deleting those Intermediate glTF bodies when Mesh is deleted, without deleting clip Assets.
- `reparentEntry` already has registry path-map bugs; delete must not rely on it — use explicit unregister + refresh/rebuild.
