## ADDED Requirements

### Requirement: Phase 6 product modifier C-ABI
The engine C-ABI SHALL expose lean entry points for PaperMouth `openAmount` (and essential config as needed), SkeletonAttachModifier child/bone binding, and LookAt target/bone drives. ABI version SHALL bump when these entry points are added. NativeAbi completeness tables SHALL include the new symbols. The ABI SHALL NOT be required to mirror every Inspector-only field.

#### Scenario: Managed host sets openAmount
- **WHEN** Blunder.Api sets PaperMouth `openAmount` on a valid Object
- **THEN** the call reaches the native modifier and subsequent sample shows the jaw effect
