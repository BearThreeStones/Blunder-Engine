## ADDED Requirements

### Requirement: Native-registered C-ABI table for managed calls
Blunder.Api SHALL invoke Object, Behaviour, property, and lifecycle C-ABI operations through a native-registered function-pointer table rather than loading a second `blunder_engine_c` image via process-default `DllImport` when a script host is running in-process with `engine_runtime`.

#### Scenario: Register before use
- **WHEN** ScriptHost receives a complete non-null C-ABI function-pointer table from native DotNetHost start
- **THEN** subsequent Blunder.Api native calls use those pointers

#### Scenario: Call before register fails clearly
- **WHEN** Blunder.Api invokes a C-ABI operation before the table is registered
- **THEN** the call fails with a clear error (or throws) and MUST NOT silently `DllImport` a second ObjectDB image

#### Scenario: Editor and managed share one ObjectDB
- **WHEN** the editor process registers pointers from its process-linked C-ABI and attaches a Behaviour to a scene ObjectId via ScriptHost
- **THEN** native `ObjectDB::get` for that id observes the Behaviour peer and frame `LifecycleDispatch::invokeTick` reaches the managed Behaviour
