## ADDED Requirements

### Requirement: Behaviour operations on C-ABI
The C-ABI SHALL expose entry points to add/remove Behaviours on an Object by ObjectId, query Behaviour count and ids, and set/get a Behaviour Script Peer pointer, without exposing C++ Object layout.

#### Scenario: Add Behaviour via C-ABI
- **WHEN** a caller adds a Behaviour type name on a valid ObjectId through the C-ABI
- **THEN** a non-zero BehaviourId is returned and Behaviour count increases by one

#### Scenario: Set peer via C-ABI
- **WHEN** a caller sets a Script Peer pointer for a valid BehaviourId
- **THEN** get-peer returns that pointer until cleared or the Behaviour is removed

### Requirement: Vec3 properties through C-ABI
The C-ABI SHALL support get/set of Vec3 ClassDB properties (at least Object position) by ObjectId for managed bindings.

#### Scenario: Position round-trip via C-ABI
- **WHEN** a caller sets Object position to (1,2,3) through the C-ABI Vec3 setter and then gets it
- **THEN** the returned components match the written values

### Requirement: ABI version for script host
The C-ABI version constant SHALL be greater than or equal to 2 when Behaviour and Vec3 entry points are present.

#### Scenario: Version reports v2+
- **WHEN** a caller queries `blunder_engine_abi_version`
- **THEN** the returned value is >= 2

## MODIFIED Requirements

### Requirement: Lifecycle hooks registerable through C-ABI
The C-ABI SHALL expose register/clear entry points for type-level Ready and Tick hooks without requiring a managed host. When hooks are registered and an Object has Behaviour Script Peers, invokeReady/invokeTick SHALL call the hook once per non-null peer.

#### Scenario: Ready hook via C-ABI
- **WHEN** a native caller sets a Ready hook through the C-ABI and invokes Ready on an Object with a Behaviour Script Peer
- **THEN** the registered hook runs with that peer

#### Scenario: Multiple peers with Tick hook
- **WHEN** a Tick hook is registered and an Object has two Behaviour peers
- **THEN** invokeTick calls the hook twice, once per peer
