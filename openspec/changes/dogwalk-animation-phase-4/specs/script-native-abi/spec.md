## ADDED Requirements

### Requirement: Blunder.Api AnimationTree surface
Blunder.Api SHALL expose managed façades for AnimationTree activation and the narrow named drive API consistent with the C-ABI. NativeAbi completeness for these entries SHALL be tested. Sync Group Fire from managed code SHALL honor OneShot semantics when the member's AnimationTree is active.

#### Scenario: Script travels and sets blend from Behaviour
- **WHEN** a Play-mode Behaviour calls Travel and sets a per-node BlendSpace1D scalar on AnimationTree
- **THEN** subsequent poses reflect the requested state and scalar under exclusive tree sampling

#### Scenario: Sync Fire OneShot from managed API
- **WHEN** a Behaviour Fires a Sync Group at an active-tree member
- **THEN** the member receives OneShot semantics without requiring the script to deactivate the tree first
