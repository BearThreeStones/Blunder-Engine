## Purpose

Gameplay and host-invoked C# write Console Messages through Debug.Log / LogWarning / LogError, with stacks, and Lifecycle exceptions become Error rows without killing the Play Process.

## ADDED Requirements

### Requirement: Debug API on Blunder.Api
`Blunder.Api` SHALL expose `Debug.Log`, `Debug.LogWarning`, and `Debug.LogError` taking a string. They SHALL create Console Messages with severity Log, Warning, and Error respectively, plus a Console stack captured at the call site. They SHALL NOT accept a context Object. `System.Console.WriteLine` SHALL NOT create a Console Message.

#### Scenario: Log from a Behaviour Tick
- **WHEN** a mounted Behaviour calls `Debug.Log("hello")` during Tick in the Play Process
- **THEN** the editor Console records a Log message whose text is `hello` with Play Process origin and a non-empty Console stack

#### Scenario: WriteLine is not a Console Message
- **WHEN** a Behaviour calls `System.Console.WriteLine("nope")`
- **THEN** the Console ring does not gain a row for that call

### Requirement: Native log C-ABI
The engine C-ABI SHALL expose a log entry that accepts Console severity, UTF-8 text, and optional UTF-8 Console stack. Debug API SHALL call that entry (via the registered NativeAbi table). Engine `LOG_INFO/WARN/ERROR` SHALL use the same Console recording path as this entry (without a script stack).

#### Scenario: Managed Debug uses registered log pointer
- **WHEN** ScriptHost has registered NativeAbi and `Debug.LogWarning` runs
- **THEN** the call goes through the registered log pointer and produces a Warning Console Message

### Requirement: Lifecycle exception becomes Error
An exception that escapes an engine-invoked C# entry on a Behaviour — Ready, Tick, OnMessage, and host-invoked callbacks of the same class including PoseApplied subscribers — SHALL be recorded as an Error Console Message with Console stack. That invocation SHALL abort. Other Behaviours SHALL still run. The Play Process SHALL NOT exit because of that exception.

#### Scenario: Tick throw continues siblings
- **WHEN** Object A has two Behaviours with peers and the first Tick throws
- **THEN** the Console records an Error with a stack
- **AND** the second Behaviour still receives Tick
- **AND** the Play Process remains running

#### Scenario: PoseApplied throw does not kill Player
- **WHEN** a PoseApplied subscriber throws during Player animation sample
- **THEN** the Console records an Error
- **AND** the Play Process remains running
