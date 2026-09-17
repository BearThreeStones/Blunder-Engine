# Spec Delta

## Purpose

The Physics Kernel can attach a static triangle mesh so forest world collision matches authored mesh geometry without moving hollow trimeshes.

## ADDED Requirements

### Requirement: Static triangle mesh collider
The Physics World SHALL allow a Static RigidBody to own a triangle-mesh Collider built from a triangle list in SI metres as Physics fixed scalars. Dynamic, Kinematic, and query-only Area bodies SHALL NOT own a hollow triangle-mesh Collider.

#### Scenario: Capsule rests on static trimesh
- **WHEN** a Dynamic or Character Controller capsule is dropped onto a Static triangle mesh that includes an upward-facing triangle under it
- **THEN** the capsule does not fall through that triangle

#### Scenario: Kernel rejects dynamic trimesh attach
- **WHEN** a caller requests a triangle-mesh Collider on a Dynamic body
- **THEN** no triangle-mesh Collider is attached
- **AND** the World remains usable

### Requirement: Missing or empty triangle data is skip
When triangle data is missing or empty, the engine SHALL attach no collider for that mesh. It SHALL NOT synthesize a box from the mesh AABB. It SHALL NOT treat that skip as a hard Import or load failure for the rest of the scene.

#### Scenario: Empty mesh has no collision
- **WHEN** a Static Triangle Mesh Unique is loaded with an empty triangle list
- **THEN** that entity has no kernel collider
- **AND** scene load continues

#### Scenario: Skip is not an import abort
- **WHEN** Import processes a glTF node whose collision extras are absent
- **THEN** Import still registers the mesh Asset
- **AND** that node has no collider Unique from those extras

### Requirement: Trimesh participates in queries
Ray and primitive shapecasts SHALL be able to hit a Static triangle-mesh Collider that passes the query mask.

#### Scenario: Ray hits a static triangle
- **WHEN** a ray is cast along a path that intersects a Static triangle mesh whose layer is in the query mask
- **THEN** the query reports a hit at that intersection
