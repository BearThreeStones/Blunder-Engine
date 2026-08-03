## ADDED Requirements

### Requirement: Sync Group and CINE C-ABI
The engine C-ABI SHALL expose Sync Group lifecycle and Fire, and CINE Enter/End plus in-CINE query (and input-suppression control as applicable). ABI version SHALL bump when these entry points are added. NativeAbi completeness tables SHALL include the new symbols.

#### Scenario: Managed façade can fire a group
- **WHEN** Blunder.Api creates a Sync Group and Fires per-member instructions
- **THEN** the call reaches native AnimationPlayers without requiring AnimationTree APIs
