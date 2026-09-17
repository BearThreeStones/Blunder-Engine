# Spec Delta

## Purpose

The editor Viewport draws collision shape wireframes so authors can see Collider and Character Controller Unique volumes in the open scene.

## ADDED Requirements

### Requirement: Editor draws collision wireframes
When Engine host mode is Editor, the Viewport SHALL draw line wireframes for Collider Unique shapes (box, sphere, capsule, triangle mesh edges) and Character Controller capsules present in the open scene. Draw SHALL use Editor Overlay line presentation (offscreen 3D + Slint), not a second present-to-HWND path.

#### Scenario: Static box is visible as wire
- **WHEN** the open scene has a Static Box Collider Unique and the editor Viewport is shown
- **THEN** that box is drawn as a collision wireframe in the Viewport

#### Scenario: Character Controller capsule is visible
- **WHEN** the open scene has a Character Controller Unique and the editor Viewport is shown
- **THEN** that capsule is drawn as a collision wireframe

### Requirement: Player does not draw collision wireframes
The Player SHALL NOT draw collision wireframes, including while paused.

#### Scenario: Play view has no collision overlay
- **WHEN** Play Mode runs in the Player process
- **THEN** the Player game view does not show Collider or Character Controller wireframes
