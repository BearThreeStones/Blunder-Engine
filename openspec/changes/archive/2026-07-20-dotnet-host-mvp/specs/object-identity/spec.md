## REMOVED Requirements

### Requirement: At most one Script Peer
**Reason:** ADR 0011 adopts Unity-style multiple Behaviours per Object; a single Script Peer slot is insufficient.
**Migration:** Use Behaviour list APIs and per-Behaviour Script Peer handles (BehaviourId). Lifecycle dispatch iterates all peers.

## ADDED Requirements

### Requirement: Object hosts Behaviours / Script Peers
An Object SHALL host zero or more Behaviours, each with at most one Script Peer handle. Script Peers SHALL NOT be stored on ECS components. BehaviourId SHALL identify a Behaviour instance for peer binding and SHALL be distinct from ObjectId.

#### Scenario: Bind peer on Behaviour
- **WHEN** a Script Peer handle is set on a Behaviour that had none
- **THEN** get-peer for that BehaviourId returns that handle until cleared or the Behaviour is removed

#### Scenario: Clear peer
- **WHEN** the Script Peer handle on a Behaviour is cleared
- **THEN** lifecycle script dispatch skips that Behaviour until a new peer is bound

#### Scenario: Multiple Behaviours allowed
- **WHEN** two Behaviours are added to the same Object
- **THEN** both remain addressable by distinct BehaviourIds concurrently
