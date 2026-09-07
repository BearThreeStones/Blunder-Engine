# secondary-command-buffers Specification

## Purpose
Record shadow, forward, overlay, and SSAO draws into Vulkan secondary command buffers on the owner thread so those passes can later be recorded in parallel, without changing what the viewport shows.

## Requirements

### Requirement: Named visual passes execute secondary command buffers
When a Vulkan device exists, the engine SHALL record the following passes that run on that frame into SECONDARY command buffers, and the PRIMARY command buffer SHALL begin each of those passes with secondary contents and execute those buffers: shadow, forward color, selection outline, overlay lines, overlay line AA, SSAO, and screen overlays. GPU pick and Texture Loader copies SHALL remain PRIMARY.

#### Scenario: Viewport frame uses secondaries for scene and overlays
- **WHEN** the editor records a viewport frame that includes shadow, forward, outline, overlay lines, SSAO, and screen overlays
- **THEN** those passes SHALL be recorded as SECONDARY command buffers
- **AND** the PRIMARY SHALL execute them inside the matching render passes
- **AND** GPU pick and texture upload command buffers SHALL still be PRIMARY

### Requirement: Forward color keeps opaque, scene overlay, then transparent order
The forward color pass SHALL execute three SECONDARY command buffers in this order inside one subpass: opaque meshes, scene overlays, then transparent meshes. Pass order relative to shadow, outline, overlay lines, overlay AA, SSAO, and screen overlays SHALL stay as it is today.

#### Scenario: Blend meshes still composite over the grid
- **WHEN** a frame draws opaque meshes, a depth-tested grid, and transparent meshes
- **THEN** opaque draws SHALL run first
- **AND** scene overlays SHALL run next
- **AND** transparent draws SHALL run last
- **AND** that order SHALL be the same as before this change

### Requirement: In-flight streams do not share a secondary buffer
Viewport, Camera Preview, and Immediate (Mesh Preview) recording SHALL use distinct SECONDARY command buffers for a command buffer that is still referenced by an in-flight PRIMARY. The engine SHALL NOT re-record a SECONDARY that the current PRIMARY has already executed.

#### Scenario: Camera Preview does not reuse the viewport forward buffers
- **WHEN** one PRIMARY records the viewport forward pass and then Camera Preview in the same submit
- **THEN** Camera Preview SHALL use different SECONDARY buffers than the viewport forward pass already executed

#### Scenario: Mesh Preview immediate submit has its own buffers
- **WHEN** Mesh Preview records a forward pass on an immediate PRIMARY
- **THEN** that recording SHALL NOT reuse a Viewport or Camera Preview SECONDARY that is still in flight

### Requirement: Owner thread records; Jobs do not
SECONDARY and PRIMARY command buffer recording SHALL run on the RHI owner thread. Jobs SHALL NOT begin, record, or execute command buffers.

#### Scenario: A Job does not record Vulkan
- **WHEN** the Job System runs a Job
- **THEN** that Job SHALL NOT record a secondary command buffer or call RHI

### Requirement: No device means no secondary allocation
When the process has no Vulkan device, the engine SHALL NOT allocate a secondary command pool or secondary command buffers. Headless Editor and Headless Player SHALL still start and exit.

#### Scenario: Headless without a device allocates nothing
- **WHEN** a Headless Editor or Headless Player starts with no Vulkan device
- **THEN** no secondary command pool or secondary command buffers SHALL be created
- **AND** the process SHALL exit cleanly

### Requirement: Quit does not hang on secondary reset
Process shutdown SHALL wait until in-flight GPU work that referenced secondaries is finished (or dropped on the owner path) before resetting or destroying those buffers, and SHALL return so the process can exit.

#### Scenario: Quit with a frame in flight
- **WHEN** the author quits the Editor or Player while a recorded frame is still in flight
- **THEN** the process SHALL exit
- **AND** it SHALL NOT hang resetting or destroying secondary command buffers

### Requirement: Skipped passes stay skipped
If a pass does not run today (shadows off, authorship overlays off, SSAO off, outline off, overlay AA off), the engine SHALL NOT begin that render pass solely to execute an empty secondary. A pass that does run MAY execute a begun-and-ended secondary with no draws.

#### Scenario: Player skips authorship overlay passes
- **WHEN** the Player presents a frame
- **THEN** outline, overlay lines, overlay AA, and screen overlay passes SHALL NOT run
- **AND** shadow, forward, and SSAO SHALL still use secondaries when those passes run

### Requirement: v1 does not add worker-thread recording or D3D12 bundles
v1 SHALL keep recording on the owner thread. It SHALL NOT record command buffers from Job workers or a new render worker pool. It SHALL NOT require D3D12 command bundles. It SHALL NOT change Bindless residency, Texture Loader upload, or GPU pick behavior.

#### Scenario: D3D12 path is not this change
- **WHEN** a process uses the D3D12 backend
- **THEN** this capability SHALL NOT require secondary-equivalent bundles
- **AND** Vulkan remains the backend that records SECONDARY command buffers
