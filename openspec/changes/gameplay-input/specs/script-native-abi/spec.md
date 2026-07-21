## ADDED Requirements

### Requirement: NativeAbi includes Gameplay Input entry points
The native-registered C-ABI function-pointer table consumed by Blunder.Api SHALL include non-null Gameplay Input Move and Jump entry points whenever ScriptHost registration succeeds for ABI version 3+.

#### Scenario: Complete table includes input pointers
- **WHEN** DotNetHost registers a BlunderNativeAbi filled from process or module symbols that include Gameplay Input
- **THEN** managed `Input` polls use those pointers

#### Scenario: Incomplete table rejected
- **WHEN** registration is attempted with null Gameplay Input entry points
- **THEN** registration fails (throws or returns error) and prior incomplete registration is not accepted as complete
