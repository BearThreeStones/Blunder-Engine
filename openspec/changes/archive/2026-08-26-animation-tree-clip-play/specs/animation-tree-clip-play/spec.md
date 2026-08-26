## Purpose

Code-first Clip Play on the co-located AnimationTree: scripts replace the tree base with a Clip Binding clip without a second Unique, PlayableGraph, or Fire-slot return-to-base.

## ADDED Requirements

### Requirement: Clip Play addresses a Clip Binding
Clip Play SHALL take a **Clip Binding** logical name on the co-located AnimationTree. It SHALL NOT take a StateMachine state name, an AnimationClip GUID, or an AnimationClip Asset Reference. **Travel** and **Start** SHALL remain the state-name APIs.

#### Scenario: Play Hit by logical name
- **WHEN** the tree is active and has a complete Clip Binding named `Hit`
- **AND** a script Clip Plays `Hit`
- **THEN** the call succeeds
- **AND** the tree base samples the `Hit` clip

#### Scenario: Play by state name fails when it is not a binding
- **WHEN** the tree has a StateMachine state `Locomotion` and no Clip Binding named `Locomotion`
- **AND** a script Clip Plays `Locomotion`
- **THEN** the call fails
- **AND** the tree pose is unchanged

### Requirement: Clip Play replaces the tree base
A successful Clip Play SHALL set a **Clip Play override**: the tree base SHALL be that one clip. v1 SHALL hard-cut (no fade). The clip clock SHALL start at 0. A later Clip Play SHALL replace the previous override immediately and restart at 0, including when the logical name is the same. Clip Play SHALL NOT queue. Clip Play SHALL NOT insert on the Fire slot and SHALL NOT write Add2.

#### Scenario: Override leaves BlendSpace
- **WHEN** the current StateMachine state is a BlendSpace1D locomotion graph
- **AND** a script Clip Plays `Hit`
- **THEN** the sampled base is the `Hit` clip only
- **AND** the BlendSpace is not sampled until the override is cleared

#### Scenario: Same-name Play restarts
- **WHEN** Clip Play override is `Hit` at mid-clip
- **AND** a script Clip Plays `Hit` again
- **THEN** the `Hit` clock is 0

### Requirement: Clip Play does not auto-return
Clip Play SHALL NOT clear the override when the clip reaches its last key. Pose SHALL hold that last key. **Travel** and **Start** SHALL clear the override and drive the StateMachine again, including Travel to the remembered current state.

#### Scenario: Hold last key
- **WHEN** Clip Play override is `Hit` of length 0.5s
- **AND** the tree advances past 0.5s without Travel or Start
- **THEN** the override is still set
- **AND** the sampled base matches the last key of `Hit`

#### Scenario: Travel clears override
- **WHEN** Clip Play override is `Hit` and the remembered state is `Locomotion`
- **AND** a script Travels to `Locomotion`
- **THEN** the override is cleared
- **AND** the tree samples `Locomotion`

### Requirement: Auto-transitions suspend during override
While a Clip Play override is set, authored StateMachine transitions SHALL NOT auto-Travel. The current StateMachine state name SHALL stay unchanged and SHALL NOT be sampled. Script Travel and Start SHALL still run and SHALL clear the override.

#### Scenario: Speed edge does not cancel Hit
- **WHEN** the current state is `Locomotion` with an outgoing transition to `Run` that is true
- **AND** a script Clip Plays `Hit`
- **AND** the tree advances
- **THEN** the current state name is still `Locomotion`
- **AND** the sampled base is still `Hit`

### Requirement: Fire and Add2 still stack
While a Clip Play override is set, the Fire slot and Add2 SHALL keep their existing stacking. Fire SHALL occupy the sampled pose while active, then the override SHALL be the base again. Fire SHALL NOT clear the override. Add2 SHALL still apply after that sample. The Clip Play clock SHALL keep advancing during Fire.

#### Scenario: Fire covers then returns to Hit
- **WHEN** Clip Play override is `Hit`
- **AND** RequestOneShot or Sync Group Fire plays `SYNC-HitReact` to completion
- **THEN** during Fire the sampled pose is `SYNC-HitReact`
- **AND** after Fire ends the override is still `Hit`

### Requirement: Clip Play failure is no mutation
Clip Play SHALL fail and SHALL NOT mutate tree playback when the name is empty, the logical name is not a complete Clip Binding, or the AnimationTree is not active. It SHALL NOT auto-activate the tree.

#### Scenario: Inactive tree fails
- **WHEN** the AnimationTree is inactive and has a complete Clip Binding `Hit`
- **AND** a script Clip Plays `Hit`
- **THEN** the call fails
- **AND** no Clip Play override is set

#### Scenario: Missing binding fails
- **WHEN** the tree is active and has no Clip Binding named `Missing`
- **AND** a script Clip Plays `Missing`
- **THEN** the call fails
- **AND** the tree pose is unchanged

### Requirement: Clip Play override is session-only
Clip Play override SHALL NOT be serialized on the Scene, SHALL NOT be an AnimationTree Asset instance override, and SHALL NOT dirty Document History.

#### Scenario: Save does not persist Hit
- **WHEN** Clip Play override is `Hit`
- **AND** the scene is saved and reloaded
- **THEN** the loaded tree has no Clip Play override

### Requirement: Clip Play C-ABI and managed façade
The engine SHALL expose Clip Play on the AnimationTree C-ABI and on `Blunder.Api` AnimationTree. The C# method MAY be named `Play`; the product operation is Clip Play. Success and failure SHALL match the failure contract above.

#### Scenario: Managed Play Hit
- **WHEN** a Behaviour calls AnimationTree Play with logical name `Hit` on an active tree that binds `Hit`
- **THEN** the native Clip Play succeeds
- **AND** the tree base samples `Hit`
