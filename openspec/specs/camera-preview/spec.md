# camera-preview Specification

## Purpose

Live floating Camera Preview panel in the editor viewport: a secondary overlay-free render through a selected scene Camera Component, presented as Slint chrome (not baked into the main viewport image).

## Requirements

### Requirement: Camera Preview visibility

In Edit Mode, the editor SHALL show a **Camera Preview** floating panel over the viewport when the current selection includes at least one entity that has a **Camera Component**. The preview target entity SHALL be the primary selection if it has a Camera Component; otherwise the first selected entity that has a Camera Component (selection order). When no selected entity has a Camera Component, the panel SHALL be hidden. The Player SHALL never show Camera Preview.

#### Scenario: Select one camera

- **WHEN** exactly one Camera entity is selected
- **THEN** Camera Preview is visible and images that entity

#### Scenario: Multi-select with primary camera

- **WHEN** multiple entities are selected and the primary selection has a Camera Component
- **THEN** Camera Preview images the primary Camera entity

#### Scenario: Multi-select primary non-camera

- **WHEN** multiple entities are selected, the primary has no Camera, and another selected entity has a Camera
- **THEN** Camera Preview images the first selected entity that has a Camera Component

#### Scenario: Player has no preview

- **WHEN** the Player process is running Play Mode
- **THEN** Camera Preview is not shown

### Requirement: Hierarchy Camera icon does not drive Camera Preview
Alt+left-pointer down on a Hierarchy Camera Unique icon SHALL NOT show, hide, or change the Camera Preview target. Camera Preview SHALL continue to follow its existing selection-based visibility rules.

#### Scenario: Alt+LMB Camera icon leaves Camera Preview rule unchanged
- **WHEN** the author Alt+LMBs the Hierarchy Camera icon
- **THEN** Camera Preview does not open or retarget as a result of that gesture
- **AND** Attachment property preview for the Camera Component may open per that capability

### Requirement: Live overlay-free preview image

While Camera Preview is visible and not collapsed, the editor SHALL render the scene each frame from the target Camera Component’s world pose, vertical FOV, near clip, and far clip into a dedicated offscreen target, and present that image in the panel content area. The preview render SHALL NOT draw Editor Overlays (grid, gizmos, outline, Camera Gizmo, etc.). Projection aspect SHALL equal the preview content box width/height. The offscreen longest edge SHALL be at most 480 pixels. When the panel is collapsed, preview rendering SHALL stop.

#### Scenario: Move camera updates preview

- **WHEN** Camera Preview is visible and the target Camera entity’s transform or FOV changes
- **THEN** the preview image updates to match

#### Scenario: Collapsed stops render

- **WHEN** the user collapses Camera Preview
- **THEN** the content image is hidden and secondary render does not run

### Requirement: Floating panel chrome

Camera Preview SHALL use a Slint floating panel over the viewport tile (independent of `viewport-image` pixels) with: entity name title, drag to reposition, resize with minimum content size of approximately 160×90 and clamping inside the viewport, collapse/expand, and a menu that offers Collapse/Expand. Panel layout (position, size, collapsed) SHALL persist for the editor process only and reset to the default bottom-right placement on restart. Pointer interaction over the panel rectangle (including the collapsed title bar) SHALL NOT fall through to viewport pick or Editor Camera orbit.

#### Scenario: Default placement

- **WHEN** Camera Preview first appears in a fresh editor process
- **THEN** it is placed at the bottom-right of the viewport with a default content width around 320px

#### Scenario: Panel blocks pick

- **WHEN** the pointer clicks inside the Camera Preview panel over a mesh
- **THEN** mesh pick / camera orbit does not claim that click

### Requirement: Camera Preview uses Light Components
While Camera Preview is visible, the preview render SHALL shade the scene from Light Components in that scene. It SHALL NOT use Studio lighting, a process-global editor directional, or an ambient floor. Editor Overlays including Light Gizmo SHALL remain excluded from the preview image.

#### Scenario: Preview matches scene lights
- **WHEN** Camera Preview is visible and the scene has a Light enabled Directional Light
- **THEN** the preview image is shaded by that Light Component

#### Scenario: Preview has no Light Gizmo
- **WHEN** Camera Preview is visible and the scene has Light Components
- **THEN** the preview image does not contain Light Gizmo wires

