## ADDED Requirements

### Requirement: Blunder.Api Sync Group and CINE surface
Blunder.Api SHALL expose managed façades for Sync Group create/join/fire/destroy and CINE Enter/End / in-CINE consistent with the C-ABI. NativeAbi completeness for these entries SHALL be tested.

#### Scenario: Script fires sync from Behaviour
- **WHEN** a Play-mode Behaviour builds per-member Fire instructions and calls Sync Group Fire
- **THEN** member AnimationPlayers start at the same logical moment under hard-cut default semantics
