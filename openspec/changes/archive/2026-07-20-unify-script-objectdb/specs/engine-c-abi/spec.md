## ADDED Requirements

### Requirement: In-process CoreCLR must not dual-load ObjectDB
When a process already links `engine_runtime` (editor, runtime, or tests that own a process ObjectDB) and hosts CoreCLR, managed C-ABI traffic SHALL use the process-linked ObjectDB. Loading SHARED `blunder_engine_c` as an additional ObjectDB owner in that process for ScriptHost/Api calls is forbidden.

#### Scenario: No second ObjectDB for managed calls in editor
- **WHEN** the editor hosts ScriptHost and Blunder.Api performs Object/Behaviour C-ABI operations
- **THEN** those operations resolve through the process-linked C-ABI (registered pointers) and MUST NOT create Objects only visible inside a separately loaded SHARED `blunder_engine_c` image

## MODIFIED Requirements

### Requirement: Shared library export for DllImport
The build SHALL provide a SHARED library target (`blunder_engine_c`) that exports the C-ABI entry points for consumers that intentionally use that image as their sole ObjectDB owner (for example Approach A `dotnet_host_test`). In-process CoreCLR alongside a statically linked `engine_runtime` SHALL NOT rely on default `DllImport("blunder_engine_c")` for ObjectDB traffic.

#### Scenario: SHARED target builds
- **WHEN** the `blunder_engine_c` target is built
- **THEN** a shared library binary is produced that exports the versioned C-ABI symbols

#### Scenario: SHARED remains valid for single-image hosts
- **WHEN** a host process uses SHARED `blunder_engine_c` as its only ObjectDB image and registers that module’s exports into ScriptHost
- **THEN** managed Behaviour Tick works against Objects created through that image’s C-ABI
