# Spec Delta

## ADDED Requirements

### Requirement: Non-resident unique meshes skip; one sync does not upload the whole unique set
When a MeshRenderer’s unique Mesh Asset has no resident `GpuMesh` yet, the editor Viewport and the Player SHALL skip that MeshRenderer for that frame. They SHALL NOT draw a placeholder box for that hole. One scene-to-render sync SHALL NOT synchronously create every unique `GpuMesh` in the open scene. After a unique `GpuMesh` is resident, static opaque and alpha-clip MeshRenderers whose mesh Final contains Meshlets SHALL still submit through GPU-driven rendering. This change SHALL NOT add a MultiMesh Unique.

#### Scenario: First frame may omit not-yet-resident static meshes
- **WHEN** the Viewport draws a scene whose unique Mesh Assets are only partly GPU-resident
- **THEN** MeshRenderers with a resident `GpuMesh` draw
- **AND** MeshRenderers whose unique `GpuMesh` is missing are skipped that frame
- **AND** GPU-driven rendering is not replaced by MultiMesh Unique

#### Scenario: Scene sync does not upload every unique mesh at once
- **WHEN** a scene with many unique Mesh Assets performs a scene-to-render sync before those `GpuMesh` objects exist
- **THEN** that sync does not synchronously `GpuMesh::create` the entire unique set
- **AND** remaining unique meshes upload on later ticks
