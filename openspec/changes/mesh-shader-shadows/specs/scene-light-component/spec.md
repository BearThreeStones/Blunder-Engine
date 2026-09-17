## MODIFIED Requirements

### Requirement: Light shadows this slice
This slice SHALL cast Light shadows from Directional, Point, and Spot Lights whose contribution includes shadows and whose Object is Active in Hierarchy. Area Light SHALL NOT cast shadows.

At most one Directional SHALL cast shadows per view: the first Light enabled Directional whose contribution includes shadows, in stable EntityId order. That Directional SHALL use the directional virtual shadow map clipmap. Other Directionals SHALL still add direct light when their contribution includes illumination.

Point Lights SHALL cast through cubemaps and Spot Lights SHALL cast through 2D perspective maps, each bounded by that type's per-view shadow-map budget. Shadows only on a Point or Spot Light SHALL cast that light's shadows and SHALL add no direct light. Shadows only on an Area Light SHALL have no effect.

#### Scenario: First shadow Directional wins
- **WHEN** two Light enabled Directionals both have contribution Illuminate and shadows
- **THEN** only the earlier EntityId Directional casts Light shadows
- **AND** both still add direct light

#### Scenario: Point Shadows only occludes without lighting
- **WHEN** a Light enabled Point has contribution Shadows only and is within the Point shadow budget
- **THEN** MeshRenderers it affects receive its cubemap shadows and do not receive its direct light

#### Scenario: Spot casts a 2D map
- **WHEN** a Light enabled Spot has contribution Illuminate and shadows and is within the Spot shadow budget
- **THEN** MeshRenderers it affects can receive its 2D shadow occlusion

#### Scenario: Area still does not cast
- **WHEN** a Light enabled Area Light has contribution Illuminate and shadows
- **THEN** it adds direct light and does not cast Light shadows
