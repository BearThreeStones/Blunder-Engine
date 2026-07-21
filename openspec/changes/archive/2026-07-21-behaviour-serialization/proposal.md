## Why

The .NET host can attach and Tick Behaviours, but scenes only persist ECS entity TRS/mesh. DogWalk and real Projects need Behaviours authored on scene entities to survive save/load. CONTEXT already names this the **Behaviour serialization slice** after the host MVP (and after single-ObjectDB).

## What Changes

- Extend scene asset / `SceneEntityDefinition` with an **ordered Behaviour list** (CLR type name, stable `BehaviourId`, optional serializable property bag).
- On scene instantiate: ensure each entity that declares Behaviours has a bound **Object**, restore Behaviour slots, and when DotNetHost is running **mount** managed peers (`AttachBehaviour`) in list order.
- On scene export/save: write Behaviour lists from the Object bound to each entity (omit soft-deleted entities as today).
- Round-trip tests: serialize → deserialize → mount → Tick (or peer present) without Inspector UX.
- Document format in testing/CONTENT notes; amend ADR 0011 / CONTEXT milestone language from “next” to “in progress / shipped” when done.
- **Depends on:** `unify-script-objectdb` (single ObjectDB) — merge or stack that branch first.
- **Out of this change:** Inspector Behaviour authoring UI, ALC hot reload, Play-mode UI, full C# field reflection for every type (MVP property bag is narrow).

## Capabilities

### New Capabilities

- `behaviour-scene`: Persist and restore ordered Behaviours on scene entities; mount managed peers when the script host is running.

### Modified Capabilities

- `csharp-behaviour`: BehaviourId and type list participate in scene round-trip; mount reconstitutes Script Peers.
- `object-identity`: Scene-authored entities with Behaviours MUST have a bound Object for serialization entry.
- `dotnet-script-host`: After scene load (host running), ScriptHost mounts Behaviours declared on loaded Objects.
- `scene-edit-commands` (light): Save/export continues to omit tombstones; Behaviour data follows surviving entities only.

## Impact

- `SceneEntityDefinition` / `scene_serializer` / `scene_instance` (or bridge that creates Objects)
- Object↔Entity reverse lookup for export
- DotNetHost / ScriptHost mount-on-load path
- Tests: scene round-trip + `editor_dotnet_host_test`-style mount
- Docs: ADR 0011, CONTEXT glossary milestone, testing.md
- Prerequisite branch: `feat/unify-script-objectdb`
