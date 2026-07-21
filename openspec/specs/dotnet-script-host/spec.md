# dotnet-script-host Specification

## Purpose
In-process CoreCLR script host: load Blunder.ScriptHost and one Project game assembly, register managed Ready/Tick hooks through the C-ABI.
## Requirements
### Requirement: In-process CoreCLR script host
The engine SHALL embed CoreCLR using `nethost` / `hostfxr` and load a managed Blunder.ScriptHost assembly without requiring an out-of-process `dotnet` helper as the shipping model.

#### Scenario: Host starts with runtime present
- **WHEN** DotNetHost starts with a valid ScriptHost DLL and runtimeconfig on a machine with .NET 10
- **THEN** start succeeds and the host reports running

#### Scenario: Host start failure is non-fatal to editor
- **WHEN** DotNetHost start fails (missing runtime or DLL)
- **THEN** the engine logs the error and continues without script Behaviours rather than aborting process startup

### Requirement: Load one Project game assembly
The script host SHALL load exactly one primary Project game assembly from the Scripts build output for the open Project. In the editor/runtime process, loading SHALL target the same ObjectDB as native scene Objects (via the registered C-ABI table). A dual-ObjectDB experimental env gate SHALL NOT be required once registration is in place.

#### Scenario: Load after build
- **WHEN** a Project Scripts assembly exists under `.blunder/scripts_bin` (or the configured output path) and the host is running with the C-ABI table registered
- **THEN** loadGameAssembly succeeds and types from that assembly can be attached as Behaviours on process ObjectIds

#### Scenario: Editor load without dual-ObjectDB gate
- **WHEN** the editor starts the script host and Scripts output exists (subject only to the Play/host-start policy such as `BLUNDER_DOTNET_SCRIPTS`)
- **THEN** loadGameAssembly is attempted without requiring `BLUNDER_DOTNET_LOAD_SCRIPTS`

### Requirement: Managed lifecycle hooks register through C-ABI
Blunder.ScriptHost SHALL register Ready and Tick hooks through the C-ABI so native LifecycleDispatch can invoke managed Behaviour methods via Script Peer handles (GCHandles).

#### Scenario: Tick reaches managed Behaviour
- **WHEN** a Behaviour is attached with a Script Peer and LifecycleDispatch invokeTick runs on its Object
- **THEN** the managed Behaviour Tick method executes once for that peer

### Requirement: Host installs C-ABI table before game assembly load
DotNetHost SHALL register the process-appropriate C-ABI function-pointer table into ScriptHost after ScriptHost loads and before `loadGameAssembly` or `attachBehaviour`.

#### Scenario: Registration order
- **WHEN** DotNetHost start succeeds
- **THEN** the C-ABI table is installed before any game assembly load or Behaviour attach export is used

#### Scenario: Test host uses SHARED image table
- **WHEN** `dotnet_host_test` LoadLibrary’s SHARED `blunder_engine_c` for Approach A
- **THEN** it fills the registration table from that module’s exports so managed calls and native invoke share one ObjectDB

### Requirement: Scene load can mount Behaviours through ScriptHost
When DotNetHost is running and a Project game assembly is loaded, scene instantiate SHALL request ScriptHost attachment for each declared Behaviour type on bound Objects so managed peers exist before the first frame Tick that should run them.

#### Scenario: Attach during load
- **WHEN** scene load finishes with host running and a declared type exists in the game assembly
- **THEN** AttachBehaviour (or equivalent) has been invoked for that ObjectId and type and the peer is bound

