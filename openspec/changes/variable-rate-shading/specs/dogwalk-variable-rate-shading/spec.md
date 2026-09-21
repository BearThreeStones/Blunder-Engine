# dogwalk-variable-rate-shading Specification

## Purpose

Human acceptance for image-based VRS is DogWalk `se-world.scene.asset` in windowed `engine_editor` and Play, not Test, Sponza, or collision QC.

## ADDED Requirements

### Requirement: DogWalk windowed walk is the acceptance scene
Human acceptance for this change SHALL use the DogWalk Project main scene `assets/Scenes/se-world.scene.asset` in windowed `engine_editor` and windowed Play. Test and Sponza SHALL NOT be the demo Project. Collision QC `root.scene.asset` SHALL NOT be this walk. 138 Spot lights, a profiler speedup number, and the dog walking SHALL NOT be required. This change SHALL NOT modify or merge PR 24 or PR 36.

#### Scenario: Editor Viewport walk
- **WHEN** the human opens DogWalk `se-world.scene.asset` in windowed `engine_editor`
- **AND** the device has fragment shading rate attachments
- **THEN** the Viewport SHALL remain clustered deferred
- **AND** the lighting pass SHALL show mixed 1×1 / 2×2 on the rate-mask overlay (edges vs flats), not a single full-screen rate

#### Scenario: Play walk
- **WHEN** the human runs Play on that same scene
- **THEN** Play SHALL be deferred clustered lighting
- **AND** Play SHALL NOT show the rate-mask overlay
