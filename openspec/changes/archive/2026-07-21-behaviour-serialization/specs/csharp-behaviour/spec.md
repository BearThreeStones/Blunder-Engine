## ADDED Requirements

### Requirement: BehaviourId survives scene round-trip
BehaviourId values written into a scene asset SHALL be restored onto the host Object on load so the same ids remain addressable after deserialize (with the Object’s next-id counter advanced past restored ids).

#### Scenario: Ids stable after load
- **WHEN** Behaviours with ids 1 and 3 are saved and the scene is reloaded
- **THEN** the Object exposes BehaviourIds 1 and 3 (not renumbered to 1 and 2 solely by index)
