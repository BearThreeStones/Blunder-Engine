## ADDED Requirements

### Requirement: Camera Preview uses Light Components
While Camera Preview is visible, the preview render SHALL shade the scene from Light Components in that scene. It SHALL NOT use Studio lighting, a process-global editor directional, or an ambient floor. Editor Overlays including Light Gizmo SHALL remain excluded from the preview image.

#### Scenario: Preview matches scene lights
- **WHEN** Camera Preview is visible and the scene has a Light enabled Directional Light
- **THEN** the preview image is shaded by that Light Component

#### Scenario: Preview has no Light Gizmo
- **WHEN** Camera Preview is visible and the scene has Light Components
- **THEN** the preview image does not contain Light Gizmo wires
