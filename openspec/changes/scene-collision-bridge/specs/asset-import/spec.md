# Spec Delta

## ADDED Requirements

### Requirement: glTF collision extras attach or skip
When Import or glTF scene ingest sees collision extras on a mesh node (Godot blender-studio `collision_info` or the equivalent extras key that exporter writes), it SHALL attach a Static Triangle Mesh Collider Unique when those extras contain a usable triangle list. When extras are absent, empty, or unusable, that node SHALL receive no collider from this path. Import SHALL NOT synthesize a box. Import SHALL NOT abort the remainder of the import solely because collision extras are missing.

#### Scenario: Present extras become static trimesh
- **WHEN** Import reads a glTF node with usable collision extras
- **THEN** that entity has a Static Triangle Mesh Collider Unique whose triangles match those extras in metres

#### Scenario: Missing extras skip
- **WHEN** Import reads a glTF node with no collision extras
- **THEN** that node has no Collider Unique from collision extras
- **AND** Import of the mesh still succeeds
