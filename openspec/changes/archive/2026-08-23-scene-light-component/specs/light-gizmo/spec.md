## Purpose

Edit Mode shows scene Light Components in the viewport as type-shaped Light Gizmo wires so authors can see emit direction, range, cone, and Area size and click to select the light entity.

## ADDED Requirements

### Requirement: Light Gizmo draw
In Edit Mode with Editor Overlays enabled, the editor SHALL draw a Light Gizmo for every entity that has a Light Component. Shape SHALL follow type: a Directional arrow along the Light emit axis; a Point sphere whose radius is Light range; a Spot outer-cone along the emit axis; an Area rectangle in local XY. Unselected lights SHALL draw muted. A single selected light SHALL use the selection color. This slice SHALL NOT expose drag handles for cone angles or Area size. The Player SHALL never draw Light Gizmos.

#### Scenario: All lights visible
- **WHEN** the active scene has two Light Component entities and none is selected
- **THEN** both Light Gizmos are drawn in the muted style

#### Scenario: Selected light uses selection color
- **WHEN** exactly one Light entity is selected
- **THEN** that Light Gizmo uses the selection color

### Requirement: Light Gizmo pick
Pointer hits on a Light Gizmo SHALL select that light entity and SHALL be handled before mesh viewport pick. When a Camera Gizmo and a Light Gizmo both hit, the closer hit SHALL win. This slice SHALL NOT start cone or Area-size handle drags from the Light Gizmo.

#### Scenario: Click light over mesh
- **WHEN** the pointer clicks a Light Gizmo that overlaps a mesh in screen space
- **THEN** the Light entity is selected and mesh pick does not claim the click

#### Scenario: Closer of camera and light
- **WHEN** a Camera Gizmo and a Light Gizmo both intersect the pick ray
- **THEN** the closer gizmo’s entity is selected

### Requirement: Player has no Light Gizmo
The Player SHALL NOT draw or hit-test Light Gizmos, including while Play Pause is active.

#### Scenario: Player frame has no Light Gizmo
- **WHEN** the Player presents a frame
- **THEN** Light Gizmos are not drawn
