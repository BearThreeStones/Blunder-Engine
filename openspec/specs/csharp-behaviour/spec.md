# csharp-behaviour Specification

## Purpose
Unity-style ordered Behaviour list on Object, per-Behaviour Script Peers, lifecycle dispatch, and Blunder.Api Behaviour programming surface.
## Requirements
### Requirement: Ordered Behaviour list on Object
An Object SHALL host an ordered list of zero or more Behaviours. Each Behaviour SHALL have a stable BehaviourId distinct from ObjectId and EntityId, a type name string, and at most one Script Peer handle. Duplicate type names on the same Object SHALL be allowed.

#### Scenario: Add multiple Behaviours
- **WHEN** an Object adds Behaviours "Motor", "Bark", and a second "Motor"
- **THEN** the Behaviour count is 3, each has a distinct BehaviourId, and list order matches insertion order

#### Scenario: Remove Behaviour
- **WHEN** a BehaviourId is removed from an Object
- **THEN** that id is no longer addressable and subsequent Tick does not invoke its peer

### Requirement: Lifecycle runs per Behaviour peer in list order
Ready and Tick SHALL be invoked once per Behaviour with a non-null Script Peer, in the Object's Behaviour list order, via the type-level lifecycle hook table.

#### Scenario: Two peers both Tick
- **WHEN** an Object has two Behaviours with Script Peers and invokeTick runs
- **THEN** the Tick hook is called once for each peer in list order

#### Scenario: Null peer skipped
- **WHEN** a Behaviour slot has no Script Peer
- **THEN** invokeTick does not call the lifecycle hook for that slot

### Requirement: Blunder.Api Behaviour programming surface
The Engine API assembly (Blunder.Api, `net10.0`) SHALL expose a Behaviour base type that holds a host Object handle and can query sibling Behaviours on that Object (`GetBehaviour` / `GetBehaviours`). Behaviour SHALL NOT be an Object subclass and SHALL NOT be an ECS Component.

#### Scenario: GetBehaviour finds sibling
- **WHEN** two Behaviours of different types are attached to the same Object
- **THEN** one Behaviour can obtain the other via GetBehaviour of that type on the host Object handle

### Requirement: Editor-attached Behaviours tick with scene Objects
When CoreCLR is hosted inside the editor/runtime process, Behaviours attached through ScriptHost to an ObjectId that exists in the process ObjectDB SHALL receive Ready/Tick from the same native `LifecycleDispatch` walk that the engine frame uses for scene Objects.

#### Scenario: Scene Object managed Tick
- **WHEN** a valid editor/runtime ObjectId receives an attached managed Behaviour and the host is running
- **THEN** the next native invokeTick on that Object executes the managed Behaviour Tick method

### Requirement: BehaviourId survives scene round-trip
BehaviourId values written into a scene asset SHALL be restored onto the host Object on load so the same ids remain addressable after deserialize (with the Object’s next-id counter advanced past restored ids).

#### Scenario: Ids stable after load
- **WHEN** Behaviours with ids 1 and 3 are saved and the scene is reloaded
- **THEN** the Object exposes BehaviourIds 1 and 3 (not renumbered to 1 and 2 solely by index)

