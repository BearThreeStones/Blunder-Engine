# Human acceptance — gpu-driven-rendering

Status is **Not run** until the human walks the story. The agent must not mark these passed.

| # | User story | Status |
|---|------------|--------|
| 1 | Open the Test project’s `sponza.scene.asset`; the atrium is in the viewport (existing Sponza mesh, a camera, and a directional light). The engine repo does not contain a copy of the Crytek files. | Not run |
| 2 | Orbit the atrium: Meshlets that leave the frustum or face away are not submitted; the hall in front is not missing chunks. | Not run |
| 3 | Stand behind a pillar: occluded geometry is Hi-Z culled; stepping out restores it via the late pass without a frame of holes. | Not run |
| 4 | On a GPU without mesh shaders, Sponza still draws (compute cull + vertex/fragment indirect), not a black viewport. | Not run |
| 5 | Open a small scene with a skinned character: skinning, gizmos, pick, and outline still use the CPU path; static props still use GPU-driven rendering. | Not run |
| 6 | Play the Sponza scene: the directional light’s shadow covers the visible atrium, not a 256-draw truncated remnant. | Not run |
