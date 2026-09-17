## Purpose

Submit static opaque and alpha-clip geometry by GPU Meshlet culling and indirect draws, with mesh shaders when the device supports them and a compute plus vertex/fragment path otherwise, while shading stays on the existing Forward and Deferred Render Paths.

## ADDED Requirements

### Requirement: Static opaque defaults to GPU-driven rendering
The editor viewport and the Player SHALL submit static opaque and alpha-clip MeshRenderers through GPU-driven rendering when that MeshRenderer’s mesh Final contains Meshlets. Skinned MeshRenderers, blend-transparent draws, pick, outline, Mesh Preview, Camera Preview, and Scene Thumbnail SHALL keep the CPU draw list. A mesh without Meshlets SHALL use the CPU draw list for that MeshRenderer.

#### Scenario: Static prop is GPU-driven
- **WHEN** the viewport draws a static opaque MeshRenderer whose cooked mesh contains Meshlets
- **THEN** that geometry is submitted by GPU-driven rendering
- **AND** it is not truncated by the Forward mesh draw cap

#### Scenario: Skinned stays on the CPU list
- **WHEN** the viewport draws an opaque skinned MeshRenderer
- **THEN** that draw uses the CPU draw list
- **AND** the Forward mesh draw cap still applies to that list

#### Scenario: Missing Meshlets fall back
- **WHEN** a static MeshRenderer’s mesh has no Meshlet payload (Fast Path or legacy Final)
- **THEN** that MeshRenderer uses the CPU draw list

### Requirement: Meshlets at Cook
Cook of a static mesh Asset SHALL write Meshlets into that mesh’s Final. Each Meshlet SHALL contain at most 64 vertices and 124 triangles and SHALL store a bounding sphere and facing cone. Skinned mesh Cook MAY omit Meshlets. Loading a Meshlet-bearing Final SHALL NOT require regenerating Meshlets on the tick.

#### Scenario: Recook produces Meshlets
- **WHEN** a static mesh Asset is Cooked
- **THEN** the Final contains Meshlets with sphere and cone bounds

#### Scenario: Tick does not build Meshlets
- **WHEN** a GPU-driven frame draws a cooked static mesh
- **THEN** the process does not build Meshlets for that mesh on the tick

### Requirement: GPU frustum, cone, and Hi-Z cull
GPU-driven rendering SHALL cull Meshlets against the camera frustum and facing cones, and SHALL occlude Meshlets with Hi-Z built from the previous frame’s offscreen depth. Early surviving Meshlets SHALL draw first. Meshlets rejected by last-frame Hi-Z SHALL be tested again in a late pass after depth has updated. The first frame SHALL treat Hi-Z as empty and SHALL draw frustum- and cone-visible Meshlets. GPU-driven rendering SHALL NOT use a depth prepass as its occlusion method. GPU-driven rendering SHALL NOT write a visibility buffer.

#### Scenario: Back-facing Meshlets stay out
- **WHEN** the camera looks at a closed static mesh whose Meshlets have facing cones
- **THEN** Meshlets that face away are not rasterized

#### Scenario: Occlusion then late restore
- **WHEN** a static Meshlet is hidden behind closer geometry in the previous frame
- **AND** the camera then reveals that Meshlet
- **THEN** the late pass draws it
- **AND** the viewport does not keep a hole for that Meshlet after that frame

#### Scenario: First frame still draws
- **WHEN** the first GPU-driven frame of a session has no previous depth pyramid
- **THEN** frustum- and cone-visible Meshlets still rasterize

### Requirement: Mesh shader or compute plus vertex/fragment
When the Vulkan device exposes `VK_EXT_mesh_shader`, GPU-driven rendering SHALL process surviving Meshlets with task and mesh shaders. When that extension is absent, GPU-driven rendering SHALL cull with compute and SHALL rasterize with traditional vertex and fragment shaders and indirect draws. A device without mesh shaders SHALL NOT produce a black viewport solely for that reason. Compute for this path SHALL run on the existing graphics queue.

#### Scenario: Mesh shaders when present
- **WHEN** the device exposes `VK_EXT_mesh_shader`
- **THEN** GPU-driven Meshlets rasterize through task and mesh shaders

#### Scenario: Compute path without the extension
- **WHEN** the device does not expose `VK_EXT_mesh_shader`
- **AND** a static Meshlet-bearing mesh is in view
- **THEN** the mesh still appears through compute cull and vertex/fragment indirect draws

### Requirement: Directional shadows use GPU-driven static geometry
When the Directional shadow pass runs, static opaque and alpha-clip MeshRenderers that use GPU-driven rendering in the main view SHALL also submit through GPU-driven rendering for that shadow map. Blend-transparent and skinned shadow casters SHALL keep the existing CPU shadow list. Shadow Meshlet cull SHALL use the shadow projection; it SHALL NOT require Hi-Z.

#### Scenario: Sponza-scale shadow is not 256-truncated
- **WHEN** more than 256 static opaque MeshRenderers cast on the Directional shadow map
- **THEN** GPU-driven static casters are not truncated by the Forward mesh draw cap

### Requirement: Shading stays Forward or Deferred
GPU-driven geometry SHALL light through the existing Forward Render Path fragment shading, or through the Deferred Render Path G-buffer then lighting when that path is active. GPU-driven rendering SHALL NOT replace those paths with a visibility-buffer resolve.

#### Scenario: Forward still lights GPU-driven meshes
- **WHEN** the editor viewport uses the Forward Render Path
- **AND** a GPU-driven static mesh is visible
- **THEN** that mesh is lit by the Forward Render Path

#### Scenario: Deferred still lights GPU-driven meshes
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** a GPU-driven static mesh is visible
- **THEN** that mesh writes the G-buffer and is lit by Deferred lighting

### Requirement: Sponza demo scene lives in the Test project
The Test project SHALL contain a Scene Asset that instances the existing Sponza mesh with a Main Camera and a Directional Light. The Blunder Engine repository SHALL NOT add the Crytek Sponza source or Intermediate tree.

#### Scenario: Scene opens the atrium
- **WHEN** the author opens the Test project Sponza Scene Asset
- **THEN** the viewport shows the Sponza atrium with a camera and a directional light

#### Scenario: Engine repo has no Crytek copy
- **WHEN** this change is in the Blunder Engine repository
- **THEN** that repository does not contain the Crytek Sponza model files
