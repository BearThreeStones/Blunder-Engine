## ADDED Requirements

### Requirement: Host installs C-ABI table before game assembly load
DotNetHost SHALL register the process-appropriate C-ABI function-pointer table into ScriptHost after ScriptHost loads and before `loadGameAssembly` or `attachBehaviour`.

#### Scenario: Registration order
- **WHEN** DotNetHost start succeeds
- **THEN** the C-ABI table is installed before any game assembly load or Behaviour attach export is used

#### Scenario: Test host uses SHARED image table
- **WHEN** `dotnet_host_test` LoadLibrary’s SHARED `blunder_engine_c` for Approach A
- **THEN** it fills the registration table from that module’s exports so managed calls and native invoke share one ObjectDB

## MODIFIED Requirements

### Requirement: Load one Project game assembly
The script host SHALL load exactly one primary Project game assembly from the Scripts build output for the open Project. In the editor/runtime process, loading SHALL target the same ObjectDB as native scene Objects (via the registered C-ABI table). A dual-ObjectDB experimental env gate SHALL NOT be required once registration is in place.

#### Scenario: Load after build
- **WHEN** a Project Scripts assembly exists under `.blunder/scripts_bin` (or the configured output path) and the host is running with the C-ABI table registered
- **THEN** loadGameAssembly succeeds and types from that assembly can be attached as Behaviours on process ObjectIds

#### Scenario: Editor load without dual-ObjectDB gate
- **WHEN** the editor starts the script host and Scripts output exists (subject only to the Play/host-start policy such as `BLUNDER_DOTNET_SCRIPTS`)
- **THEN** loadGameAssembly is attempted without requiring `BLUNDER_DOTNET_LOAD_SCRIPTS`
