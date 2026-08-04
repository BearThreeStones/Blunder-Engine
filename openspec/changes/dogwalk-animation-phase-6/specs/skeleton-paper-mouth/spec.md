## ADDED Requirements

### Requirement: PaperMouth jaw openAmount
The engine SHALL provide a ClassDB **PaperMouth** SkeletonModifier that applies a post-sample transform to a configured jaw bone from an `openAmount` scalar. Authors and scripts SHALL be able to set `openAmount` without Behaviour Tick for Edit preview. PaperMouth SHALL NOT require mesh blendshapes or a full facial rig for Phase 6 Done.

#### Scenario: openAmount changes jaw pose
- **WHEN** PaperMouth is enabled on an Object with a named jaw bone and `openAmount` changes from 0 to a positive value
- **THEN** the jaw bone local pose after the modifier chain differs in a detectable way from the closed pose

#### Scenario: Edit scrub without Behaviour Tick
- **WHEN** an author scrubs `openAmount` in Edit Mode without DotNetHost / Behaviour Tick
- **THEN** the Skeleton reflects the PaperMouth result after preview sample

### Requirement: Optional attach-driven openAmount
PaperMouth MAY support an optional mode that writes the same `openAmount` scalar from Attach occupancy / prop-in-mouth logic. When the mode is disabled, only explicit `openAmount` writes apply.

#### Scenario: Optional mode off
- **WHEN** attach-driven mode is disabled and a child is attached
- **THEN** `openAmount` does not change solely because of Attach
