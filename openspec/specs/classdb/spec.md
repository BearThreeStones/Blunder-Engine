# classdb Specification

## Purpose
Runtime ClassDB: single type database for exported classes, Clang-generated registration and API Blueprint, PtrCall/Variant call paths, and type-level lifecycle hooks.

## Requirements

### Requirement: ClassDB is the single type database
The engine SHALL maintain one ClassDB that records exported classes, properties, methods, and component schemas for both editor tooling and future script bindings.

#### Scenario: Lookup exported class
- **WHEN** a caller queries ClassDB for an exported type name that was registered at startup
- **THEN** ClassDB returns that type's metadata including its properties and methods

#### Scenario: Unexported type absent
- **WHEN** a caller queries ClassDB for a type that was not marked for export
- **THEN** ClassDB reports the type as unknown

### Requirement: Clang generator produces registration and API Blueprint
The build SHALL run a Clang AST–based generator over annotated headers to emit ClassDB registration code and a language-neutral API Blueprint artifact. The generator SHALL NOT require changing the engine's primary compiler away from MSVC.

#### Scenario: Pilot types regenerate
- **WHEN** annotated pilot headers change and the generator runs
- **THEN** updated registration sources and an API Blueprint file are produced that describe those exported types

### Requirement: PtrCall is the primary dynamic call path
ClassDB method invocation for script-oriented calls SHALL use PtrCall (typed argument/return slots without Variant boxing). A Variant path MAY exist for editor property get/set and serialization glue only.

#### Scenario: PtrCall invokes exported method
- **WHEN** a caller invokes an exported method through PtrCall with correctly typed slots
- **THEN** the native method runs and writes the return value into the provided return slot when applicable

#### Scenario: Property via Variant for tooling
- **WHEN** the editor gets or sets an exported property through the Variant property API
- **THEN** the value round-trips through Variant without requiring the caller to use PtrCall

### Requirement: Lifecycle hooks are type-level
Exported lifecycle slots (at least Ready and Tick) SHALL be dispatched through a per-type replaceable hook table via PtrCall on each Script Peer present on an Object's Behaviour list, in list order. The kernel SHALL support clearing and replacing the entire type's hooks as a unit. Multiple Behaviours per Object (ADR 0011) SHALL each receive lifecycle calls when they have a Script Peer.

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
