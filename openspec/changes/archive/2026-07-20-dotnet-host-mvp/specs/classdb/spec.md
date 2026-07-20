## MODIFIED Requirements

### Requirement: Lifecycle hooks are type-level
Exported lifecycle slots (at least Ready and Tick) SHALL be dispatched through a per-type replaceable hook table via PtrCall on each Script Peer present on an Object's Behaviour list, in list order. The kernel SHALL support clearing and replacing the entire type's hooks as a unit. Multiple Behaviours per Object SHALL each receive lifecycle calls when they have a Script Peer.

#### Scenario: No peer skips script lifecycle
- **WHEN** Tick runs for an Object with no Behaviour Script Peers
- **THEN** no script lifecycle PtrCall is issued for that Object

#### Scenario: Hooks cleared
- **WHEN** the type-level lifecycle hooks for an exported type are cleared
- **THEN** subsequent lifecycle dispatch for Objects of that type does not call previous hooks

#### Scenario: Pilot TRS properties registered
- **WHEN** ClassDB initializes with the generated Object pilot registration
- **THEN** Object metadata includes name, enabled, position, rotation, and scale properties plus the set_name method

#### Scenario: Multi-Behaviour Tick order
- **WHEN** an Object has two Behaviours with Script Peers and Tick is invoked
- **THEN** the lifecycle Tick hook runs once per peer in Behaviour list order
