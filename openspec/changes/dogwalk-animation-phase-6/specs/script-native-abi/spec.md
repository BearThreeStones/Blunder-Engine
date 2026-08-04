## ADDED Requirements

### Requirement: Blunder.Api Phase 6 modifier surface
Blunder.Api SHALL expose managed façades for lean PaperMouth, SkeletonAttachModifier, and LookAt product drives consistent with the C-ABI. NativeAbi completeness for these entries SHALL be tested.

#### Scenario: Script drives mouth and attach
- **WHEN** a Play-mode Behaviour sets PaperMouth `openAmount` and configures SkeletonAttachModifier child/bone
- **THEN** after animation sample the jaw pose and child Object Transform reflect those drives
