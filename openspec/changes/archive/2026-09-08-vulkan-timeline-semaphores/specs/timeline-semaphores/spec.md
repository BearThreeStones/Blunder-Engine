## Purpose

Owner-thread CPU waits and polls for in-flight viewport, immediate, and GPU pick work use a Vulkan timeline semaphore so a later compute queue can wait on the same counter, without changing what the viewport shows.

## ADDED Requirements

### Requirement: Device timeline is required when a Vulkan device exists
When the process creates a Vulkan device, that device SHALL enable timeline semaphores. If the physical device cannot enable them, device creation SHALL fail and SHALL NOT continue with binary-fence-only waits as a silent fallback.

#### Scenario: Missing timeline feature fails start
- **WHEN** a Vulkan backend starts on a physical device that cannot enable timeline semaphores
- **THEN** device creation SHALL fail
- **AND** the process SHALL NOT run the viewport on that device with fence-only waits

### Requirement: Owner-thread waits use timeline values
When a Vulkan device exists, the engine SHALL signal a monotonic timeline value from graphics-queue submits that today wait or poll a fence for: viewport in-flight frames (including readback and zero-copy present), Camera Preview present, immediate owner-thread submits that wait, and GPU pick. The RHI owner thread SHALL poll or wait those values. Jobs SHALL NOT wait on the timeline.

#### Scenario: Viewport presents without stalling the tick on an in-flight frame
- **WHEN** the editor records a viewport frame while the previous slot is still on the GPU
- **THEN** the owner thread SHALL NOT wait that in-flight work to completion before returning from the tick
- **AND** present (readback or zero-copy) SHALL still occur after that work’s timeline value is reached
- **AND** the viewport SHALL look as it does today

#### Scenario: GPU pick still completes off the tick
- **WHEN** the author clicks a mesh in the viewport
- **THEN** GPU pick SHALL still return a result on a later frame
- **AND** the tick SHALL NOT stall waiting for that pick submit
- **AND** pick results SHALL match today’s hit behavior

#### Scenario: A Job does not wait on GPU
- **WHEN** the Job System runs a Job
- **THEN** that Job SHALL NOT wait or poll the timeline semaphore

### Requirement: No device means no timeline allocation
When the process has no Vulkan device, the engine SHALL NOT allocate a timeline semaphore. Headless Editor and Headless Player SHALL still start and exit.

#### Scenario: Headless without a device allocates nothing
- **WHEN** a Headless Editor or Headless Player starts with no Vulkan device
- **THEN** no timeline semaphore SHALL be created
- **AND** the process SHALL exit cleanly

### Requirement: Quit does not hang on timeline wait
Process shutdown SHALL wait until in-flight GPU work that signaled the timeline is finished (or dropped on the owner path) before destroying the timeline semaphore, and SHALL return so the process can exit.

#### Scenario: Quit with a frame in flight
- **WHEN** the author quits the Editor or Player while a recorded frame or an immediate upload is still in flight
- **THEN** the process SHALL exit
- **AND** it SHALL NOT hang waiting on the timeline semaphore

### Requirement: v1 does not add a compute queue
v1 SHALL keep a single graphics queue as today. It SHALL NOT create a dedicated compute queue, move SSAO / pick / cull onto compute, wait from a Job, require D3D12 changes, or change Bindless residency, secondary command buffer recording, or Slint Present.

#### Scenario: Still one graphics queue
- **WHEN** a Vulkan device is created for the editor or player
- **THEN** the engine SHALL NOT create a second queue for async compute in this change
- **AND** named visual passes SHALL still record as they do after secondary command buffers
