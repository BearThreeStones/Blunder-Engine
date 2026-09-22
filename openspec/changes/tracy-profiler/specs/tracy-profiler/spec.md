# tracy-profiler Specification

## Purpose

Give windowed Editor and Player a Frame timing HUD, give the editor a self-drawn Profiler dock on an engine ring, and optionally dual-write the same instrumentation to standalone Tracy — without embedding Tracy UI or making Tracy the panel source.

## ADDED Requirements

### Requirement: Two product layers; Tracy is not a product layer
The product SHALL provide a Frame timing HUD (Layer 1) and an editor Profiler dock (Layer 2). Tracy Client SHALL be optional dual-write to a standalone Tracy process. Connecting Tracy SHALL NOT be required to read HUD or dock numbers. The product SHALL NOT treat a Tracy window as Layer 1 or Layer 2.

#### Scenario: HUD without Tracy
- **WHEN** a windowed Editor Session has the Frame timing HUD on
- **AND** Tracy Client is not compiled in, or no Tracy process is connected
- **THEN** the HUD SHALL show CPU frame time, GPU frame time, and main Pass times from the engine ring

#### Scenario: Dock without Tracy
- **WHEN** a windowed Editor Session has the Profiler dock open
- **AND** no Tracy process is connected
- **THEN** the dock SHALL show a frame strip and Pass/zone lanes from the engine ring

### Requirement: Frame timing HUD is a Slint overlay with the same numbers on Editor and Player
Windowed `engine_editor` Viewport and windowed `engine_player` SHALL share one Frame timing HUD readout: FPS, CPU frame milliseconds, GPU frame milliseconds, a main Pass table, packed GPU-driven instance count, MeshBatch count, surviving meshlet-indirect command count (`cmds`, not `vkCmd*`), and light count. `inst` and `cmds` SHALL NOT be filled from the same CPU submit-list length. The HUD SHALL be Slint. Headless Editor, Headless Player, `--mcp`, and Project Manager SHALL NOT show the HUD. The HUD SHALL NOT be an Editor Overlay (gizmo / grid / outline family). The HUD SHALL NOT be the 2027-01 game shell.

#### Scenario: Editor Viewport HUD
- **WHEN** the author enables the Frame timing HUD in windowed `engine_editor`
- **THEN** the Viewport SHALL show CPU ms, GPU ms, main Pass times, packed `inst`, `batches`, surviving `cmds`, and light count

#### Scenario: Player HUD matches Editor numbers
- **WHEN** windowed `engine_player` and windowed `engine_editor` run the same scene on the same device
- **AND** both have the HUD on
- **THEN** both HUDs SHALL show the same class of numbers (FPS, CPU ms, GPU ms, main Passes, packed `inst` / `batches` / surviving `cmds`, lights)

#### Scenario: Headless has no HUD
- **WHEN** a Headless Editor or Headless Player is running
- **THEN** no Frame timing HUD SHALL be shown

### Requirement: HUD default off, F3, not persisted
The Frame timing HUD SHALL start off on a fresh process. F3 SHALL toggle it on windowed Editor and windowed Player. The editor Viewport menu SHALL also toggle it. The on state SHALL NOT persist across a fresh launch. There SHALL NOT be a product settings page for the HUD. F11 SHALL remain RenderDoc in-app capture and SHALL NOT toggle the HUD.

#### Scenario: Fresh launch is off
- **WHEN** a windowed Editor Session starts
- **THEN** the Frame timing HUD SHALL be off
- **AND** the Viewport SHALL look like today’s viewport without a stats overlay

#### Scenario: F3 toggles
- **WHEN** the author presses F3 in a windowed Editor or windowed Player
- **THEN** the Frame timing HUD SHALL toggle on or off

### Requirement: Profiler dock is editor-only Slint on the engine ring
Windowed `engine_editor` SHALL provide a Profiler dock: a frame strip, thread and GPU Pass lanes, and click-to-inspect zone name and milliseconds. The dock SHALL use Editor Theme and SHALL default to a bottom dock under the viewport (Animation Window / Console family). It SHALL NOT cover the Viewport when closed. It SHALL NOT appear in Player, Headless, `--mcp`, or Project Manager. Data SHALL come from the Frame timing ring. The dock SHALL NOT embed Tracy View, ImGui, egui, Qt, or a WebView as a Slint component. The dock SHALL NOT require `TRACY_ENABLE`.

#### Scenario: Dock shows lanes from the ring
- **WHEN** the author opens the Profiler dock with no Tracy process connected
- **THEN** the dock SHALL show a frame strip and Pass/zone lanes
- **AND** clicking a zone SHALL show that zone’s name and milliseconds

#### Scenario: Player has no dock
- **WHEN** a windowed Player is running
- **THEN** the Player SHALL NOT show the Profiler dock

### Requirement: Frame timing ring is engine-owned and ~120 frames
Each `tickOneFrame` SHALL append one ring slot with CPU frame milliseconds, GPU frame milliseconds, named Pass GPU times, a small set of CPU zones (tick, Job, scene sync), and packed instance / MeshBatch / surviving meshlet-cmd counts plus light count. Capacity SHALL be about 120 frames. Older slots SHALL drop. HUD and dock SHALL read this ring. The ring SHALL work when Tracy Client is not compiled in. HUD and dock SHALL NOT read Tracy internal buffers, the Tracy protocol, or `.tracy` files.

#### Scenario: Ring wraps
- **WHEN** more than about 120 ticks have been recorded
- **THEN** the ring SHALL still hold about 120 most recent frames
- **AND** the HUD SHALL still show the latest slot

### Requirement: Tracy Client is optional dual-write, not the panel source
When `TRACY_ENABLE` is defined, the same instrumentation SHALL also emit Tracy zones, `FrameMark`, GPU zones, and count plots to a Client that a standalone Tracy process may connect to. When `TRACY_ENABLE` is not defined, HUD and dock SHALL still function. `TRACY_ENABLE=0` SHALL NOT be treated as off — only absence of the define is off. Enabled Client builds SHALL also define `TRACY_ON_DEMAND`, `TRACY_ONLY_LOCALHOST`, and `TRACY_NO_BROADCAST`. The Client SHALL link into the static engine runtime only. The process SHALL NOT compile Tracy GUI. The install package SHALL NOT include `Tracy.exe`. CI and shipping Release SHALL NOT define `TRACY_ENABLE`.

#### Scenario: Unconnected Client does not fill memory
- **WHEN** a development build defines `TRACY_ENABLE` and `TRACY_ON_DEMAND`
- **AND** no Tracy process is connected
- **THEN** the process SHALL NOT accumulate unbounded Tracy events
- **AND** HUD and dock SHALL still show numbers

#### Scenario: Shipping has no listen
- **WHEN** a shipping build is produced
- **THEN** `TRACY_ENABLE` SHALL NOT be defined
- **AND** the process SHALL NOT listen for a Tracy connection
- **AND** the Frame timing HUD SHALL still be available

### Requirement: FrameMark is the engine tick
When Tracy Client is compiled in, each `tickOneFrame` SHALL emit one `FrameMark`. Present (Skia or SDL) SHALL NOT be that frame mark. Idle and interactive Present pacing SHALL NOT change for this profiler.

#### Scenario: Tick is the frame
- **WHEN** Tracy Client is compiled in and a Tracy process is connected
- **AND** the editor is idle with few Presents
- **THEN** Tracy SHALL still record one `FrameMark` per engine tick

### Requirement: GPU zones are named Passes plus a few internals
HUD Pass table, ring, and Tracy GPU zones SHALL share Pass-level granularity: live Frame graph names `viewport.gbuffer` and `viewport.lighting` (editor Deferred), or `viewport.scene` (Forward / today’s Player), plus present optional `viewport.ssao`, `viewport.volumetric_fog`, and Sink `viewport.copy`. A small number of internal GPU zones MAY record shadow fill, GPU-driven cull, froxel fill, and the lighting fullscreen triangle. The engine SHALL NOT emit a GPU zone per draw, per MeshRenderer, per instance, or per Spot light. Instance and light counts SHALL appear on the HUD; when Client is compiled in they SHALL also `TracyPlot`. Engine GPU timestamps SHALL come from an engine query pool so shipping needs no Client. When Client is compiled in, `TracyVkCollect` SHALL run after Frame graph `execute`, before `vkQueueSubmit`, on the PRIMARY, outside a render pass.

#### Scenario: Lighting Pass has a GPU time
- **WHEN** the editor Viewport records Deferred clustered lighting
- **THEN** the HUD Pass table SHALL include a GPU time for `viewport.lighting`
- **AND** there SHALL NOT be one GPU zone per Spot light

#### Scenario: Player Forward Pass is `viewport.scene`
- **WHEN** a windowed Player records a Forward viewport frame
- **THEN** the HUD Pass table SHALL include `viewport.scene`
- **AND** this change SHALL NOT convert that Player frame to Deferred

### Requirement: Device log and integrated-GPU warning
After physical-device selection, the log SHALL include the selected `deviceName` and `deviceType`. When the selected device is not DISCRETE, the Frame timing HUD SHALL mark GPU timings unreliable. Linux Merge CI SHALL NOT fail because GPU milliseconds look wrong.

#### Scenario: Discrete name is logged
- **WHEN** the process selects a Vulkan device
- **THEN** the log SHALL contain that device’s name and type

#### Scenario: Integrated GPU is flagged
- **WHEN** the selected device is not a discrete GPU
- **AND** the Frame timing HUD is on
- **THEN** the HUD SHALL warn that GPU timings are unreliable
