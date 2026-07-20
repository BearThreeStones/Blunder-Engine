## ADDED Requirements

### Requirement: In-process CoreCLR script host
The engine SHALL embed CoreCLR using `nethost` / `hostfxr` and load a managed Blunder.ScriptHost assembly without requiring an out-of-process `dotnet` helper as the shipping model.

#### Scenario: Host starts with runtime present
- **WHEN** DotNetHost starts with a valid ScriptHost DLL and runtimeconfig on a machine with .NET 10
- **THEN** start succeeds and the host reports running

#### Scenario: Host start failure is non-fatal to editor
- **WHEN** DotNetHost start fails (missing runtime or DLL)
- **THEN** the engine logs the error and continues without script Behaviours rather than aborting process startup

### Requirement: Load one Project game assembly
The script host SHALL load exactly one primary Project game assembly from the Scripts build output for the open Project.

#### Scenario: Load after build
- **WHEN** a Project Scripts assembly exists under `.blunder/scripts_bin` (or the configured output path) and the host is running
- **THEN** loadGameAssembly succeeds and types from that assembly can be attached as Behaviours

### Requirement: Managed lifecycle hooks register through C-ABI
Blunder.ScriptHost SHALL register Ready and Tick hooks through the C-ABI so native LifecycleDispatch can invoke managed Behaviour methods via Script Peer handles (GCHandles).

#### Scenario: Tick reaches managed Behaviour
- **WHEN** a Behaviour is attached with a Script Peer and LifecycleDispatch invokeTick runs on its Object
- **THEN** the managed Behaviour Tick method executes once for that peer
