## ADDED Requirements

### Requirement: Player Play session mounts scene Behaviours
When the Player loads the Play entry scene with DotNetHost running and a game assembly loaded, Behaviour mount rules SHALL apply in that process the same as in-process mount-when-host-running: bound Objects restore slots and AttachBehaviour runs in list order.

#### Scenario: Entry scene Behaviours Tick in Player
- **WHEN** the Play entry scene declares a Behaviour available in Scripts and Play is running (not paused)
- **THEN** the Player’s Objects have non-null peers and invokeTick reaches the managed Behaviour
