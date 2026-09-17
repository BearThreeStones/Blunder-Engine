# Spec Delta

## ADDED Requirements

### Requirement: C-ABI physics query and Character Controller
The engine C-ABI SHALL expose physics raycast and primitive shapecast entries, entity group get/set/find, and Character Controller MoveAndSlide / velocity / floor flags, without requiring a managed host. ABI version SHALL be at least 13 when these entries ship.

#### Scenario: ABI version is at least 13
- **WHEN** `blunder_engine_abi_version` is queried after this change
- **THEN** the returned value is >= 13

#### Scenario: Native ray without managed host
- **WHEN** a native test builds a World with a Static box and raycasts through the C-ABI
- **THEN** the call reports a hit without a script host
