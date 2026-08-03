## ADDED Requirements

### Requirement: AnimationTree C-ABI
The engine C-ABI SHALL expose AnimationTree activation, named drive controls (Travel/Start, per-node BlendSpace1D scalar, RequestOneShot, Add2 weight/clip as applicable), and any queries needed for Edit scrub / completeness. ABI version SHALL bump when these entry points are added. NativeAbi completeness tables SHALL include the new symbols.

#### Scenario: Managed façade can drive BlendSpace
- **WHEN** Blunder.Api sets a BlendSpace1D scalar by node logical name on an active AnimationTree
- **THEN** the call reaches native tree sampling without requiring Godot parameter-path APIs
