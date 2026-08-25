## MODIFIED Requirements

### Requirement: Placement Preview matches spawned shading
Placement Preview SHALL use the same opaque Mesh Asset materials and Light Components in the open scene as the MeshRenderer that drop will spawn. It SHALL NOT use Studio lighting, a hidden editor directional, ghost alpha, or wireframe as the product look. With no Light enabled Light Component in the scene, the preview SHALL have no hidden directional.

#### Scenario: Preview uses scene materials
- **WHEN** Placement Preview is visible for a Mesh Asset that has materials
- **THEN** the preview is shaded with those materials in the editor viewport scene pass

#### Scenario: Preview uses scene Light Components
- **WHEN** Placement Preview is visible and the open scene has a Light enabled Directional Light
- **THEN** the preview is shaded by that Light Component, not Studio lighting
