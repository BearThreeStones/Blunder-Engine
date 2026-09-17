## Purpose

Load color material textures without stalling the engine tick on disk decode or a GPU fence wait, while draws keep sampling the Bindless fallback until the copy is resident.

## ADDED Requirements

### Requirement: CPU read and decode run as Jobs
The Texture Loader SHALL Submit file read and pixel decode as Jobs over caller-owned Job data. Those Jobs SHALL NOT call RHI, Object, ClassDB, SceneInstance, or Slint. The engine tick SHALL NOT enter a Job barrier in order to show a frame with fallback textures.

#### Scenario: Decode does not freeze the viewport
- **WHEN** the author opens a scene whose material textures are not yet GPU-resident
- **THEN** the viewport keeps presenting
- **AND** those textures are decoded as Jobs into Job data
- **AND** the tick does not wait on a Job barrier for that decode before presenting

### Requirement: GPU copy does not wait on the tick
After Job data holds decoded pixels, the Texture Loader SHALL record a copy into a GPU image, submit that copy without waiting the tick, and recycle staging memory only after the GPU signals completion. v1 SHALL submit those copies on the existing graphics queue. v1 SHALL NOT require a dedicated transfer queue family.

#### Scenario: Copy submit does not wait the fence on the tick
- **WHEN** decoded pixels are ready for a material texture
- **THEN** the tick SHALL submit the copy without waiting that copy's fence before presenting
- **AND** staging bytes for that copy SHALL stay allocated until the GPU signals completion

### Requirement: Draws use Bindless fallback until the copy completes
Until the GPU signals that a material texture copy finished, mesh draws that would sample that texture SHALL use the Bindless fallback index. After that signal, those draws SHALL sample the resident table index for that texture.

#### Scenario: First frames show fallback then the real texture
- **WHEN** the author opens a scene with several large material textures
- **THEN** the first frames sample the Bindless fallback for those textures
- **AND** later frames sample the uploaded images
- **AND** the viewport does not freeze on decode or a full-device fence wait

### Requirement: In-flight requests coalesce
While a texture path is already in flight (CPU Job or GPU copy), a second request for that same path SHALL NOT start a second upload.

#### Scenario: Duplicate path uploads once
- **WHEN** the same material texture path is requested again before the first request is GPU-resident
- **THEN** the engine SHALL perform at most one GPU upload for that path

### Requirement: Cancel on scene drop
When the live scene is dropped or replaced while texture requests are in flight, the process SHALL remain stable. Completions for dropped requests SHALL NOT write GPU images that belong to the dropped scene.

#### Scenario: Scene switch drops in-flight uploads
- **WHEN** the author switches scenes while texture uploads are still in flight
- **THEN** the process SHALL NOT crash
- **AND** a completion for the old scene SHALL NOT write a GPU image that was dropped with that scene

### Requirement: Quit joins in-flight work
Process shutdown SHALL stop new texture requests, finish or drop in-flight CPU Jobs and GPU copies on the owner path, and return so the process can exit.

#### Scenario: Quit does not hang
- **WHEN** the author quits the Editor or Player while texture uploads are still in flight
- **THEN** the process SHALL exit
- **AND** it SHALL NOT hang waiting on a GPU fence or Job Worker join

### Requirement: Headless without a device skips GPU upload
When the process has no Vulkan device, the Texture Loader SHALL NOT submit GPU copies. Headless Editor and Headless Player SHALL still start and exit.

#### Scenario: Headless skips GPU upload
- **WHEN** a Headless Editor or Headless Player starts with no Vulkan device
- **THEN** the Texture Loader SHALL NOT submit GPU copies
- **AND** the process SHALL exit cleanly

### Requirement: v1 is color material textures only
v1 SHALL apply this path to color material textures used with the Bindless table. It SHALL NOT upload mesh vertex or index buffers, SHALL NOT replace ThumbnailGenerator, Pull cook, or the Engine GPU cache, and SHALL NOT add a C-ABI or Blunder.Api texture schedule.

#### Scenario: Mesh uploads stay on the existing path
- **WHEN** a mesh is uploaded for a draw
- **THEN** that vertex and index upload SHALL NOT go through the Texture Loader
