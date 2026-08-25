## Purpose

Authors place lights in the Scene Asset as a native Light Component so the editor viewport, Player, Camera Preview, and Placement Preview shade from document lights instead of a hidden editor rig.

## ADDED Requirements

### Requirement: Light Component on scene entities
The engine SHALL support a native Light Component on scene entities as a Unique attachment (at most one per entity). Pose SHALL follow that entity’s TRS. Type SHALL be a field on the Component: Directional Light, Point Light, Spot Light, or Area Light. Every Light Component SHALL have Light color (linear RGB, default white), Light intensity (scalar multiplier, default 1), Light enabled (default on), and Light contribution (default Illuminate and shadows). The Component SHALL NOT be a C# Behaviour. Adding Light SHALL NOT create a bound Object.

#### Scenario: Round-trip Light Component
- **WHEN** an entity has a Light Component with type, color, intensity, enabled, and contribution
- **THEN** scene serialize and load preserve those fields

#### Scenario: Absent light key
- **WHEN** an entity JSON object has no `"light"` key
- **THEN** the loaded entity has no Light Component

### Requirement: Light emit axis
Directional Light, Spot Light, and Area Light SHALL emit along the entity local -Z axis (the same look axis as the Camera Gizmo). Point Light SHALL have no emit axis. Direction SHALL NOT be a separate stored vector on the Light Component.

#### Scenario: Identity rotation Directional emits world -Z
- **WHEN** a Directional Light entity has identity rotation
- **THEN** its emit direction is world -Z

### Requirement: Type-specific Light fields
Point Light and Spot Light SHALL have a positive Light range. Spot Light SHALL have inner and outer cone angles in degrees with `0 ≤ inner < outer ≤ 90` (defaults inner 0, outer 45). Area Light SHALL be a rectangle in local XY, centered on the origin, with positive width (local X) and height (local Y) on the Component, not entity scale. Directional Light SHALL have no range. Area Light SHALL have no range in this slice.

#### Scenario: Spot cone defaults
- **WHEN** the author adds a Light and sets type to Spot Light without editing cone angles
- **THEN** inner is 0 degrees and outer is 45 degrees

#### Scenario: Area size is not entity scale
- **WHEN** an Area Light has width 2 and height 1 and the entity scale is (3, 3, 3)
- **THEN** the authored emitting rectangle remains width 2 and height 1

### Requirement: Live views use only Light Components
The editor viewport, Player, Camera Preview, and Placement Preview SHALL shade only from Light Components in that scene. They SHALL NOT use a process-global editor directional, an ambient floor, or Studio lighting. A scene with no Light enabled Light Component SHALL have no hidden fill light. Mesh Preview Render SHALL continue to use Studio lighting. Scene Thumbnail Render SHALL use Light Components when present and SHALL fall back to Studio lighting when the scene has none.

#### Scenario: Zero lights is dark
- **WHEN** the open scene has no Light enabled Light Component and a lit MeshRenderer is visible in the editor viewport
- **THEN** shading has no hidden directional and no ambient floor

#### Scenario: Mesh Preview stays studio
- **WHEN** Mesh Preview Render draws a Mesh Asset
- **THEN** it uses Studio lighting, not Light Components from the open scene

### Requirement: Light enabled and contribution
A Light Component with Light enabled off SHALL contribute neither illumination nor shadows. Light contribution SHALL be one of: Illuminate and shadows, Illuminate only, Shadows only. Shadows only SHALL add no direct light and SHALL only cast that light’s shadows. Shadows only SHALL NOT be represented as intensity 0 or as Light enabled off.

#### Scenario: Disabled light is ignored
- **WHEN** a Directional Light is Light enabled off
- **THEN** it does not illuminate and does not cast Light shadows

#### Scenario: Shadows only adds no radiance
- **WHEN** a Light enabled Directional has contribution Shadows only and it is the shadow-casting Directional
- **THEN** MeshRenderers it affects receive its shadows and do not receive its direct light

### Requirement: Light linking
Each Light Component SHALL have an optional inclusive receiver list of MeshRenderer entities. An empty list SHALL mean the light affects every MeshRenderer in the scene, including newly spawned meshes. A non-empty list SHALL mean only those MeshRenderers are affected (illumination and/or that light’s shadows). Missing or non-MeshRenderer entries SHALL be ignored. Authors SHALL edit the list in the Inspector Light section. This slice SHALL NOT provide a viewport light-link mode.

#### Scenario: Empty linking lights a new mesh
- **WHEN** a Directional Light has an empty receiver list and the author spawns a MeshRenderer
- **THEN** that MeshRenderer is affected by the Directional Light

#### Scenario: Non-empty linking excludes others
- **WHEN** a Point Light’s receiver list contains only entity A and entity B is another MeshRenderer
- **THEN** A is affected by that Point Light and B is not

### Requirement: Light evaluation cap
For each MeshRenderer, the engine SHALL evaluate at most 8 Light enabled lights that affect that MeshRenderer under Light linking and whose contribution is not a no-op for that draw. The engine SHALL take the first 8 in stable EntityId order and drop the rest. A scene MAY contain more than 8 lights.

#### Scenario: Ninth affecting light is dropped
- **WHEN** nine Light enabled Point Lights all have empty linking lists
- **THEN** a MeshRenderer is shaded by the first 8 in stable EntityId order and not the ninth

### Requirement: Light shadows this slice
This slice SHALL cast Light shadows only from Directional Lights whose contribution includes shadows. At most one such Directional SHALL cast shadows per view: the first Light enabled Directional whose contribution includes shadows, in stable EntityId order. Other Directionals SHALL still add direct light when their contribution includes illumination. Point, Spot, and Area SHALL NOT cast shadows. Shadows only on a non-Directional light SHALL have no effect in this slice.

#### Scenario: First shadow Directional wins
- **WHEN** two Light enabled Directionals both have contribution Illuminate and shadows
- **THEN** only the earlier EntityId Directional casts Light shadows
- **AND** both still add direct light

### Requirement: Distance falloff
Point Light and Spot Light intensity SHALL fall inverse-square with distance and SHALL reach 0 at Light range, with a smooth window near range. Directional Light SHALL have no distance falloff. Area Light SHALL NOT use this falloff in this slice.

#### Scenario: Beyond range is dark from that Point
- **WHEN** a surface is farther from a Point Light than Light range
- **THEN** that Point Light does not illuminate the surface

### Requirement: Area Light is a front-facing rectangle
Area Light shading SHALL be a front-facing rectangle (emit-axis side only), not a Point Light with a quad gizmo. Width and height SHALL change the lighting. The back face SHALL NOT contribute. This slice SHALL NOT require LTC or GGX area specular and SHALL NOT cast Area shadows.

#### Scenario: Enlarging Area softens lighting
- **WHEN** an Area Light’s width and height increase while pose and intensity stay the same
- **THEN** the lighting on a nearby MeshRenderer changes rather than only the Light Gizmo changing size

### Requirement: New Scene default Directional
A New Scene Asset SHALL include a second default entity with a Directional Light, not on the Main Camera entity. That Directional SHALL be placed above the XY ground with its Light emit axis slanted toward that plane. The starter SHALL NOT include meshes.

#### Scenario: New Scene has camera and light
- **WHEN** the author creates a New Scene Asset
- **THEN** the document has a Main Camera entity and a separate Directional Light entity
- **AND** the Directional is not on the Main Camera entity

### Requirement: Inspector Light section
When the selection has a Light Component, the Inspector SHALL show a Light section for type, color, intensity, enabled, contribution, type-specific fields, Remove, and the Light linking receiver list. Changing type SHALL keep the same Unique Light Component.

#### Scenario: Change type stays Unique
- **WHEN** the author adds Light (Directional) then sets type to Point Light
- **THEN** the entity still has one Light Component of type Point Light
- **AND** Add… Light remains disabled
