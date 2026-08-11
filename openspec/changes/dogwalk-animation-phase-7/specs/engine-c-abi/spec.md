## ADDED Requirements

### Requirement: Phase 7 AnimationTree C-ABI
The engine C-ABI SHALL expose entry points to get/set AnimationTree **tree parameters** (bool/float by name) and any additional queries/setters required for transition-aware Edit/Play hosts. ABI version SHALL bump when these entry points are added. NativeAbi completeness tables SHALL include the new symbols.

#### Scenario: Managed host sets tree param
- **WHEN** Blunder.Api sets a float tree parameter by name on an Object's AnimationTree
- **THEN** the native tree stores that value for transition evaluation
