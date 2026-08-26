## ADDED Requirements

### Requirement: C-ABI AnimationTree Clip Play
The engine C-ABI SHALL expose a Clip Play entry on AnimationTree (logical clip name). Success SHALL set the Clip Play override. Failure SHALL match the Clip Play failure contract (empty name, missing binding, inactive tree). ABI version SHALL be at least 12 when this entry ships.

#### Scenario: ABI version is at least 12
- **WHEN** `blunder_engine_abi_version` is queried after this change
- **THEN** the returned value is >= 12

#### Scenario: Native Clip Play without managed host
- **WHEN** a native caller Clip Plays a bound logical name on an active AnimationTree
- **THEN** the call succeeds and the tree base samples that clip
