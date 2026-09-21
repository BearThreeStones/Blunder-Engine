## MODIFIED Requirements

### Requirement: Light evaluation cap
For each MeshRenderer shaded by a non-clustered forward path (Player, Camera Preview, Placement Preview, Mesh Preview, Scene Thumbnail), the engine SHALL evaluate at most 8 Light enabled lights that affect that MeshRenderer under Light linking and whose contribution is not a no-op for that draw. The engine SHALL take the first 8 in stable EntityId order and drop the rest. A scene MAY contain more than 8 lights.

The editor Viewport's clustered GBuffer lighting path is exempt from this flat 8-light cap for point and spot lights: it is bounded instead by the per-froxel light cap defined by the froxel grid capability. Directional lighting in the editor Viewport continues to go through the existing fullscreen deferred pass and is unaffected by either cap.

#### Scenario: Ninth affecting light is dropped
- **WHEN** nine Light enabled Point Lights all have empty linking lists and a MeshRenderer is shaded by a non-clustered forward path (for example, the Player)
- **THEN** that path shades the MeshRenderer using the first 8 in stable EntityId order and not the ninth

#### Scenario: Editor Viewport is not limited to 8
- **WHEN** nine Light enabled Point Lights all have empty linking lists, none share a froxel with more than 64 assigned lights, and a MeshRenderer is visible in the editor Viewport
- **THEN** the editor Viewport's clustered GBuffer lighting path can shade that MeshRenderer using contributions from more than 8 of those Point Lights
