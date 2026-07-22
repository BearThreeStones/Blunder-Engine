## ADDED Requirements

### Requirement: Behaviour OnMessage lifecycle peer hook
Alongside Ready and Tick, the ScriptHost SHALL register a process-wide message hook so native MessageDispatch can invoke managed `Behaviour.OnMessage` for each Script Peer. Ready/Tick semantics SHALL remain unchanged.

#### Scenario: Message hook registered with lifecycle hooks
- **WHEN** ScriptHost RegisterLifecycleHooks (or equivalent startup) runs
- **THEN** native MessageDispatch has a non-null message hook before game code Sends

#### Scenario: OnMessage does not replace Tick
- **WHEN** a Behaviour implements both Tick and OnMessage
- **THEN** frame Tick still runs via LifecycleDispatch and Messages run only through MessageDispatch
