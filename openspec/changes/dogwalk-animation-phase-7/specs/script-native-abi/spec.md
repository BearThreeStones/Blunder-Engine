## ADDED Requirements

### Requirement: Blunder.Api tree parameters
Blunder.Api SHALL expose managed façades to get/set AnimationTree tree parameters consistent with the C-ABI. NativeAbi completeness for these entries SHALL be tested. Existing Travel/Start/BlendSpace/OneShot/Add2 façades SHALL remain available alongside transitions.

#### Scenario: Behaviour sets param for transition
- **WHEN** a Play-mode Behaviour sets a tree parameter that satisfies an authored transition
- **THEN** auto Travel can occur without that Behaviour calling Travel for the same switch
