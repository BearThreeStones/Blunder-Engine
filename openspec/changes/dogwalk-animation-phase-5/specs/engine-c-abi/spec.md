## ADDED Requirements

### Requirement: Phase 5 animation C-ABI
The engine C-ABI SHALL expose SkeletonModifier enable/order hooks as applicable, method-track listener or query surface as needed for managed hosts, BlendSpace2D parameter setters/getters, and AnimationTree Asset bind/query entry points. ABI version SHALL bump when these entry points are added. NativeAbi completeness tables SHALL include the new symbols.

#### Scenario: Managed host sets BlendSpace2D
- **WHEN** Blunder.Api sets a BlendSpace2D `(x,y)` by node logical name
- **THEN** the call reaches native tree sampling without requiring Godot parameter-path APIs
