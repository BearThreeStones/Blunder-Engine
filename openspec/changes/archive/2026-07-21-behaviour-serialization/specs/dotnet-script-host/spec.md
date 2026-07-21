## ADDED Requirements

### Requirement: Scene load can mount Behaviours through ScriptHost
When DotNetHost is running and a Project game assembly is loaded, scene instantiate SHALL request ScriptHost attachment for each declared Behaviour type on bound Objects so managed peers exist before the first frame Tick that should run them.

#### Scenario: Attach during load
- **WHEN** scene load finishes with host running and a declared type exists in the game assembly
- **THEN** AttachBehaviour (or equivalent) has been invoked for that ObjectId and type and the peer is bound
