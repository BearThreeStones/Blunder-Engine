## ADDED Requirements

### Requirement: Active AnimationTree owns Skeleton sampling
When an Object's AnimationTree is active, Skeleton sampling for that Object SHALL come from the AnimationTree. AnimationPlayer two-slot / Play / Crossfade SHALL NOT write bones concurrently. Phase 1 co-located Player↔Skeleton pairing and Phase 2 two-slot behaviour remain when the tree is inactive.

#### Scenario: Mutual exclusion while active
- **WHEN** AnimationTree is active on an Object that also has an AnimationPlayer
- **THEN** only the tree writes the Skeleton pose for that Object that frame

### Requirement: Dominant-clip step clock under AnimationTree
Under an active AnimationTree, PoseApplied SHALL still fire after sampling. The playback position used for Animation step detection SHALL be the base dominant clip clock as defined by the animation-tree capability. Phase 2 dominant-slot rules remain for inactive-tree two-slot playback.

#### Scenario: Tree step clock available to scripts
- **WHEN** AnimationTree is active and PoseApplied fires during BlendSpace1D locomotion
- **THEN** scripts can read a dominant-clip playback position suitable for Stepped frametime-modulo logic
