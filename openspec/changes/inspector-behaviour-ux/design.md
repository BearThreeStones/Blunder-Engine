## Context

Behaviour serialization (ADR 0011 follow-on) already persists `SceneBehaviourDeclaration` and mounts peers in Player. Edit Mode has no product DotNetHost (ADR 0014). ADR 0016 locks Inspector authoring as **declaration-authoritative** with a build-time **Behaviour type catalog**. Today `Object` Behaviour slots lack a property bag, so `exportToScene` drops properties. Inspector Transform already uses Slint + History Commands on `EntityId`.

## Goals / Non-Goals

**Goals:**
- Inspector section to Add/Remove/reorder Behaviours and edit bool/number/string fields
- Type catalog from Scripts DLL metadata after successful build
- Property bags live on Object slots and round-trip on save
- History Commands for Add/Remove/reorder/property commit
- Missing-type broken UI; no host required for authoring

**Non-Goals:**
- Vec3/enum/nested/asset editors
- Live peer as Edit Mode source of truth
- AttachBehaviour as the Inspector edit API
- ALC hot reload
- Hand-typed CLR names as primary Add

## Decisions

### D1 — Declaration store on Object slots
**Choice:** Extend `BehaviourSlot` with `eastl::vector<SceneBehaviourProperty> properties` (or equivalent). Instantiate copies bag from scene; export reads bag from Object.
**Why:** Single live document for Inspector + save; matches ADR 0016.
**Rejected:** Edit only `Scene` asset in memory without Object slots.

### D2 — Catalog via managed scanner + JSON cache
**Choice:** After `ScriptsBuilder` success, run a small `net10.0` tool (`Blunder.ScriptsCatalog`) that loads the game DLL metadata and writes `.blunder/behaviour_catalog.json`. Native loads that file for Add + forms.
**Why:** No editor CoreCLR product host; reuse build gate.
**Rejected:** Start DotNetHost only to reflect types; parse IL in C++.

### D3 — Ensure Object on first Add
**Choice:** `SceneInstance::ensureBoundObject(EntityId)` creates/binds Object when Add runs on an entity with empty list.
**Why:** Serialization already requires Object for non-empty behaviours.

### D4 — History mirrors Transform sealing
**Choice:** Push Commands after Add/Remove/reorder drop / property Enter-or-focus-loss. Intermediate typing does not push.
**Why:** Matches existing Document History MVP.

### D5 — Reorder API on Object
**Choice:** `bool Object::moveBehaviour(size_t from, size_t to)` (or BehaviourId-based move) updates list order used by Ready/Tick and export.
**Why:** User chose drag reorder in grill.

## Risks / Trade-offs

- [Catalog stale] Scripts changed but not built → Mitigation: Add prompts build; missing types show broken
- [Scanner load fails] Game DLL references unresolved → Mitigation: metadata-only load / Reflection.Metadata; log + empty catalog
- [Bag bag vs catalog drift] Renamed field → Mitigation: keep bag keys; form shows catalog members; unknown bag keys retained for mount
- [Slint drag reorder complexity] → Mitigation: start with up/down if drag blocked, but grill chose drag — implement drag in Slint list

## Migration Plan

1. Slot property bag + export/import round-trip tests
2. Catalog tool + JSON + native reader
3. Editor Commands + SceneInstance ensure Object
4. Inspector Slint section + sync/apply
5. Archive OpenSpec

Rollback: revert UI; bags on slots remain backward-compatible with empty properties.

## Open Questions

None — grilled in ADR 0016 / CONTEXT.
