## Context

See proposal.md for motivation. Inspector already has three add paths (`applyInspectorAddCamera` with **no** History Command; Behaviour and SkeletonModifier Commands on `EntityId`) plus clip-map **edit** via `makeSetAnimationPlayerClipBindingsCommand`. `ensureBoundObject` and `ensureSkeleton` exist; `populateSkeletonFromSkin` lives only in `GltfSceneImporter`. Spawned meshes often have MeshRenderer and no Object. Dual-track identity is ADR 0003; Add… is ADR 0033; hydration is ADR 0034.

## Goals / Non-Goals

**Goals:**
- One Slint Add… popup dispatching to existing add ops plus new Unique-attachment ops
- Composite Editor Commands so cascade + hydration undo as one step
- Shared skeleton-from-skin helper used by Add… without spawning glTF children
- Camera add/remove become History Commands like Behaviour

**Non-Goals:**
- New ClassDB “Attachment” type or ECS Component-as-Object
- Changing Import packaging or auto-filling clip maps
- Reworking `attachSceneEntityMeshes` / `importUnderEntity` on scene load
- Multi-select Add…, clip drag-from-browser, Add… search

## Decisions

### D1 — Dispatch table, not a new runtime type
**Choice:** Slint Add… lists rows with a kind + type-name payload. Native `applyInspectorAdd` switches: Camera / Skeleton / AnimationPlayer / AnimationTree / Behaviour CLR name / SkeletonModifier type name. Reuse `applyInspectorAddBehaviour` and `applyInspectorAddSkeletonModifier`.
**Why:** Matches ADR 0033 (picker is a gesture). Behaviours stay catalog-driven, not ClassDB.
**Rejected:** Registering Behaviours in ClassDB so one reflection enum can populate the menu; Godot-style Add Node creating child Objects.

### D2 — Composite host snapshot Command
**Choice:** One `IEditorCommand` per Add… click stores which Unique attachments (and Object binding) were **newly** created, plus before/after Skeleton rest/bind if hydration ran, plus Camera presence. Undo reverses only what that click created. Behaviour / Modifier Add… clicks keep using the existing single-type Commands (no cascade).
**Why:** Spec requires one click = one undo for Tree→Player→Skeleton.
**Rejected:** Pushing three Commands internally; Memento of the whole Object.

### D3 — Hydrate via extracted glTF skin helper
**Choice:** Extract `populateSkeletonFromSkin` (or equivalent) to a shared function. On Skeleton create, resolve the entity mesh GUID/path, `openGltfImportDocument`, pick the skin used by that mesh (first skinned primitive / owning node skin), fill the **selected** Object’s Skeleton. Log a warning and leave empty bones on failure or non-skinned mesh.
**Why:** MeshAsset has no bone names; rest/bind is in Intermediate glTF (ADR 0034).
**Rejected:** Empty-only Skeleton; calling `importUnderEntity` from Add…; a new Skeleton Asset.

### D4 — Camera stays entity-only
**Choice:** Add Camera / Remove Camera touch `SceneInstance` Camera Component only. Do not `ensureBoundObject`.
**Why:** ADR 0003; Camera is already an ECS-style component. Spec forbids Object creation for Camera.
**Rejected:** Forcing Object on every Add… item for a uniform code path.

### D5 — Add clip reuses clip-bindings Command
**Choice:** Add clip appends `{name:"", guid:""}` and pushes `makeSetAnimationPlayerClipBindingsCommand` (or a thin wrapper) with before/after binding lists. Clip-row Remove is the same Command with the row omitted.
**Why:** Clip map already round-trips through that Command; empty rows need a follow-up so commit-on-focus does not no-op on empty name (today `applyInspectorAnimationClipCommit` rejects empty names).
**Rejected:** Putting clip types in Add…; requiring a GUID before the row exists.

### D6 — Empty clip row commit
**Choice:** Allow persisting a row with empty name only as the Add-clip draft; committing a still-empty name on blur SHALL NOT delete the row in this slice. Filling name+GUID uses the existing commit path. Duplicate names: last write wins on the map (current `setClipBindings` behavior) — do not add a new uniqueness UI.
**Why:** Smallest change so the first row can exist; uniqueness polish is out of slice.
**Rejected:** Modal “pick clip asset” as the only Add clip path.

### D7 — Unique-attachment Remove Commands
**Choice:** Section Remove for Camera / Skeleton / Player / Tree. Remove Player / Tree do not clear Skeleton. Remove Skeleton no-ops (UI disabled) while Player, Tree, or any Modifier remains. If Object was created solely by this Add… and Remove tears down the last ClassDB host (and no Behaviours), keep the Object if it still has Skeleton-only or leave Object in place — **keep the Object** after Remove of Unique attachments (do not destroy Object in this slice) to avoid fighting Behaviour slots and export.
**Why:** Reverse-cascade rules from grill; destroying Object is easy to get wrong with Behaviours.
**Rejected:** Destroying Object when Camera-only; deleting Skeleton while Player remains.

## Risks / Trade-offs

- [Wrong skin in multi-skin glTF] → Mitigation: bind the skin of the mesh primitive already loaded on that entity; warn if ambiguous
- [Hydration vs later `importUnderEntity` on reload] → Mitigation: this slice hydrates the **selected** Object so spawn+Add Player works in-session; scene-load child expansion is unchanged (known pre-existing split), not fixed here
- [Add Camera currently has no Command] → Mitigation: add Camera Commands as part of this change, not a silent leftover
- [Empty clip name vs existing commit guard] → Mitigation: D6; do not reuse commit-empty as delete
- [Slint popup grouping] → Mitigation: same PopupWindow pattern as Add Behaviour, with section labels as non-clickable rows

## Migration Plan

1. Shared skeleton-from-skin helper + tests (skinned vs static vs missing glTF)
2. Composite Add/Remove Commands + Camera History + Add clip empty row
3. Slint Add… popup; remove three old buttons; Unique Remove on section headers
4. Wire `slint_system` dispatch, grey Unique rows, single-select gate
5. Manual smoke: spawn skinned mesh → Add Player → Add clip → preview Play → undo

Rollback: revert Inspector UI first; Commands unused if UI gone.

## Open Questions

None — grilled into CONTEXT / ADR 0033 / ADR 0034.
