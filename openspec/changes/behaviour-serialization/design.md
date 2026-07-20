## Context

Scenes serialize `SceneEntityDefinition` (name, TRS, parent, mesh) into `.scene.asset` JSON. Runtime spawn is ECS-centric (`SceneInstance`); Objects/Behaviours are separate and today only appear via tests or explicit C-ABI attach. ADR 0011 / CONTEXT define the next slice: persist ordered Behaviour list (BehaviourId, type, serializable fields) with the scene/document, without Inspector UX or hot reload. Single-ObjectDB (`unify-script-objectdb`) is a hard prerequisite so mounted peers target process Objects.

## Goals / Non-Goals

**Goals:**
- Scene JSON round-trips Behaviour type list + BehaviourId (+ narrow property bag)
- Load mounts Objects + slots; with DotNetHost running, attaches managed peers and Ready/Tick work
- Save reads Behaviours from Object bound to entity
- Automated tests prove round-trip without editor UI

**Non-Goals:**
- Inspector add/remove/edit Behaviour UX
- ALC hot reload / peer rebind across reload
- Serializing every C# field via full reflection generator
- Play-mode button (keep `BLUNDER_DOTNET_SCRIPTS` or successor)
- Changing content layout of Scripts/ vs Assets/

## Decisions

1. **Behaviours live on scene entities in JSON, Objects at runtime**  
   Extend `SceneEntityDefinition` with `behaviours: [{ "type": "<ClrTypeName>", "id": <uint64>, "properties": { ... } }]`. At instantiate, create/bind an Object for that entity when the list is non-empty (or always for authored entities — prefer **only when behaviours non-empty** to avoid mass Object creation).  
   *Rejected:* Separate Object-only scene track; storing GCHandles or peer pointers in the asset.

2. **Persist BehaviourId**  
   Save and restore the same BehaviourId values so C-ABI / future hot reload and undo can key off id. On load, Object’s `m_next_behaviour_id` advances past max restored id.  
   *Rejected:* Index-only identity in the asset.

3. **Mount timing**  
   After entities spawn and Objects exist: if DotNetHost `isRunning()` and game assembly loaded, call `attachBehaviour(objectId, typeName)` for each slot (peer null until then). If host not running, leave slots with type names and null peers (edit-time / offline).  
   *Rejected:* Requiring host to load scene; failing scene load when Scripts missing.

4. **Property bag MVP (narrow)**  
   First slice supports a JSON object of simple values (bool/int/float/string) applied after Attach via a small managed or Variant path. C# fields without an explicit serializable convention are skipped. Full ClassDB-driven C# field export is a follow-up.  
   *Rejected:* Blocking the slice on full Blueprint→C# serializer.

5. **Export path**  
   When exporting Scene from live `SceneInstance`, for each non-tombstoned entity find bound Object (EntityId→Object); write its Behaviour list. Entities without Object omit `behaviours`.  
   *Rejected:* Only round-tripping pure JSON without live Object (insufficient for editor save after Attach).

6. **Branch dependency**  
   Implement on top of merged or stacked `unify-script-objectdb` so mount uses registered NativeAbi / process ObjectDB.

## Risks / Trade-offs

- **[No Object↔Entity index today]** → Add reverse map or scan ObjectDB by EntityId on export; document cost for large scenes (MVP OK).
- **[Type rename breaks scenes]** → Store full CLR type name; missing type on mount logs and skips peer (slot may remain).
- **[Duplicate type names]** → Allowed; order + BehaviourId disambiguate.
- **[Property bag drift]** → Keep schema version comment in scene or ignore unknown keys.

## Migration Plan

1. Land schema + serializer parse/write (empty behaviours compatible).
2. Instantiate: Object bind + native slots.
3. Mount when host running; tests.
4. Export from live Objects; editor save path.
5. Docs + ADR note.
6. Rollback: ignore unknown `behaviours` key if needed (forward compatible parse).

## Open Questions

- Always create Object per entity vs only when behaviours present — **default: only when behaviours non-empty** (revisitable if editor needs Object for every entity later).
- Whether property bag is in the same apply wave or a fast follow — **default: include minimal bag in this change so CONTEXT “serializable fields” is not empty**.
