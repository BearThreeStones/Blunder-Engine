## Context

See proposal.md for motivation. Entity Inspector still binds process-global `BlinnPhongEditorSettings` in `inspector_panel.slint` (Dir / ka / kd / ks / shininess / unlit / SSAO). `RenderSystem` copies that bag into `frame_state.shading` then forces `ssao_enabled = false`. `applyBlinnPhongToMeshUniforms` / `applyPbrToMeshUniforms` still write kd/ks/shininess/unlit from the bag on every forward draw. Lights for live views already gather Light Components when `live_scene_lighting` is set; Mesh Preview packs Studio lighting through the same struct’s light fields.

`MeshAsset` holds one `MaterialAsset`. `loadMesh` for glTF takes the first primitive; Mesh Preview and scene import expand primitives, each with its own Import MaterialAsset. Mesh descriptors persist `texture_guids` as Mesh→Texture graph edges. Global Commands already exist (`pushGlobalCommand`); Content Browser folder-ops (in flight) routes Ctrl+Z to Global History on Browser focus.

Glossary: `CONTEXT.md` (Mesh material override, Material Inspector, Mesh shading defaults, Editor shading overrides retired).

## Goals / Non-Goals

**Goals:**
- Delete the Entity Inspector shading block and stop live/preview BRDF from reading that bag
- Overlay a sparse YAML bag onto the Mesh Asset’s one MaterialAsset at load
- Material Inspector + Inspector object cells + Global Commands + Asset Inspector undo routing
- Rebuild `texture_guids` as Import ∪ non-empty override slot GUIDs

**Non-Goals:**
- Material Asset Content Browser type; per-primitive override lists; per-field Revert
- Scene environment / SSAO authorship; glTF/Source writes
- Splitting Studio lighting into a new public type unless the shading struct becomes unreadable (keep Studio light dir/color as an internal pack)

## Decisions

### D1 — Sparse YAML on the Mesh descriptor
**Choice:** Add an optional `material_override` map on `MeshAssetDescriptor`. Only authored keys exist (bools, floats, RGB, slot GUID strings). Empty string on a slot key means suppress Import texture. Absent key means Import.
**Why:** Matches grilled sparse overlay + Reimport-keep-bag. YAML stays the Mesh document.
**Rejected:** Full MaterialAsset snapshot on first edit (freezes DCC). Sidecar `.material` Asset (Material Asset type).

### D2 — Overlay at MaterialAsset construction, not per-draw
**Choice:** After Import builds the first-primitive MaterialAsset (`loadMesh` / `getMaterialAsset()`), apply the sparse bag in place (factors, unlit, texture pointers resolved from GUIDs). Primitive loads used only by Preview/import keep their own Import MaterialAsset unless they are that same first-primitive `MeshAsset`.
**Why:** One overlay site; live MeshRenderer and Mesh Preview first primitive stay consistent.
**Rejected:** Reading the YAML from `applyPbrToMeshUniforms` every draw. Stamping every primitive.

### D3 — BRDF uniforms from MaterialAsset or Mesh shading defaults
**Choice:** `applyBlinnPhongToMeshUniforms` takes kd/ks/ka/shininess/unlit from `MaterialAsset` when non-null, else Mesh shading defaults (white / spec 0.4 / shininess 32 / ka 0 / not unlit). `BlinnPhongEditorSettings` remains only for Studio light pack (dir/color) on non-`live_scene_lighting` paths. Stop copying Inspector sliders into `frame_state.shading` for BRDF. SSAO stays forced off.
**Why:** Live views already have Light Components; the leftover bug is BRDF still coming from the bag.
**Rejected:** Keeping editor kd/ks as a viewport debug overlay. Using Studio ambient as a hidden floor on live views.

### D4 — Inspector object cell, not Editor Object Field
**Choice:** New Slint Inspector object cell (22px recessed Godot row) for the four slots. Wire pick/clear/drop to the same GUID write path. Do not instantiate `EditorObjectField` inside Inspector.
**Why:** Grilled chrome. Foldout / Reset still use Editor controls (Inspector chrome exception).
**Rejected:** GUID string field. Embedding Editor Object Field.

### D5 — Global Command snapshots the sparse bag
**Choice:** One `IEditorCommand` type (or thin variants) holding Mesh GUID + before/after `material_override` (+ `texture_guids` if the write updates the union). Apply writes descriptor, reloads overlay on cached MeshAsset, refreshes Mesh Preview / in-scene MeshRenderers using that GUID. Reset is the same Command with after = empty bag. Push after successful write (`pushGlobalCommand`).
**Why:** Same mutation-then-push pattern as Browser Global Commands. Document History must not own Mesh YAML.
**Rejected:** Document Command. Immediate write with no undo.

### D6 — Focus-routed Undo includes Asset Inspector
**Choice:** Extend the Browser focus router: Inspector focus && Asset Inspector presentation → Global History. Viewport / Entity Inspector still Document History.
**Why:** Material edits are Global Commands; Ctrl+Z in Inspector must invert them.
**Rejected:** History Panel only (B). Always Document.

## Risks / Trade-offs

- [First-primitive-only looks wrong on multi-material Preview] → Accepted this slice; extra primitives stay Import (spec). Follow-up: primitive list.
- [Empty slot vs absent key] → Clear writes empty key; Reset deletes keys. Tests for both. Inspector shows None in both cases; Reset is how authors recover Import textures.
- [Cached MeshAsset not rebuilt after YAML write] → Command apply/undo must reload or mutate the live MaterialAsset and dirty preview/scene draws.
- [Unarchived content-browser-folder-ops also patches document-history] → This delta is a superset (Browser + Asset Inspector). Merge focus routing in one function if both land.
- [BlinnPhongEditorSettings still named like a product bag] → Leave rename out unless the struct becomes Studio-only and the extra BRDF fields can die in this slice (prefer delete unused BRDF fields once no caller remains).

## Migration Plan

1. Descriptors without `material_override` load as today (Import MaterialAsset only).
2. Remove Inspector shading UI and Slint bindings; leftover process values must not affect draws.
3. No scene file migration. No glTF rewrite.
4. Rollback: revert the change; YAML `material_override` keys are ignored by older builds (unknown keys should be preserved if the YAML emitter round-trips unknown maps — if the current emitter strips unknown keys, implement preserve-or-document that old editors drop the bag).

## Open Questions

None that block specs or tasks. Slot picker UX (modal vs click-to-assign from Browser selection) can follow existing Asset-reference patterns at implement time.
