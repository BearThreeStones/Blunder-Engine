## ADDED Requirements

### Requirement: Editor-attached Behaviours tick with scene Objects
When CoreCLR is hosted inside the editor/runtime process, Behaviours attached through ScriptHost to an ObjectId that exists in the process ObjectDB SHALL receive Ready/Tick from the same native `LifecycleDispatch` walk that the engine frame uses for scene Objects.

#### Scenario: Scene Object managed Tick
- **WHEN** a valid editor/runtime ObjectId receives an attached managed Behaviour and the host is running
- **THEN** the next native invokeTick on that Object executes the managed Behaviour Tick method
