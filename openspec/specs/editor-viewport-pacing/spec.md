# editor-viewport-pacing Specification

## Purpose
TBD - created by archiving change viewport-interactive-fps. Update Purpose after archive.
## Requirements
### Requirement: Interactive viewport pacing tier

While the user is actively manipulating the 3D viewport (camera orbit/pan/look, scroll zoom, or transform gizmo drag), the editor SHALL schedule viewport composite requests and Skia presents at a rate targeting **20–30 Hz** (default minimum interval **33 ms** between composite requests).

#### Scenario: Orbit camera in viewport

- **WHEN** the user holds right-mouse drag to orbit inside the central viewport for at least 2 seconds
- **THEN** the engine SHALL submit Vulkan viewport frames at an average rate of at least **18 Hz** (measured as `vkQueueSubmit` events over a 15 s Nsight capture)

#### Scenario: Transform gizmo drag

- **WHEN** the user drags a transform gizmo handle on a selected entity
- **THEN** viewport composite requests SHALL use the interactive tier interval until the drag ends plus the configured hold period

### Requirement: Idle viewport pacing tier

When the viewport is not in an interactive session (no camera manipulation, gizmo drag, or qualifying scroll input) and the interactive hold timer has elapsed, the editor SHALL throttle viewport composite requests and Skia presents to **5–10 Hz** (default minimum interval **100 ms** between composite requests).

#### Scenario: Static viewport after interaction

- **WHEN** the user releases all viewport manipulation inputs and waits at least **200 ms** without further input
- **THEN** Vulkan viewport submit rate SHALL fall to at most **12 Hz** over a 15 s Nsight capture while the scene remains unchanged

#### Scenario: Scene selection change

- **WHEN** the user changes selection or triggers `requestViewportRedraw()`
- **THEN** the engine SHALL render and request a viewport composite regardless of idle tier throttling (existing dirty/generation behavior preserved)

### Requirement: Tier-aware composite request scheduling

The system SHALL NOT apply a fixed **250 ms** minimum interval between viewport composite requests. Composite request spacing MUST be determined by the active pacing tier and the configured environment intervals.

#### Scenario: Interactive tier replaces 250 ms floor

- **WHEN** the interactive tier is active and `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_MS` is unset
- **THEN** composite requests MAY occur as frequently as once per **33 ms** (subject to other defer gates)

### Requirement: Zero-copy camera motion submits during interactive tier

On the zero-copy viewport path, when the camera changes during the interactive tier, `RenderSystem` SHALL record and submit the Vulkan off-screen pass even if a Skia composite is not yet scheduled for that frame.

#### Scenario: Camera moves between composite requests

- **WHEN** zero-copy is active, the interactive tier is active, and the camera matrix changes between composite request windows
- **THEN** `vkQueueSubmit` SHALL still occur for the off-screen pass (fence slot permitting)

#### Scenario: Idle camera-only motion suppressed

- **WHEN** zero-copy is active, the idle tier is active, and only the camera changes with no pending composite request
- **THEN** the engine MAY skip recording a new Vulkan frame (existing optimization)

### Requirement: Defer and resize behavior preserved

Viewport pacing tiers SHALL NOT bypass existing defer paths: window resize cooldown, `shouldDeferHeavyFrameWork`, dock layout present suppression, and `shouldSkipSkiaPresentDuringDefer`.

#### Scenario: Window edge resize

- **WHEN** the user is resizing the window edge and defer-heavy work is active
- **THEN** viewport pacing SHALL follow existing defer/present-skip rules without forcing interactive-tier composites

### Requirement: Environment configuration

The editor SHALL support the following environment variables for pacing (values in milliseconds; `0` disables that specific floor):

| Variable | Default | Purpose |
|----------|---------|---------|
| `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_MS` | 33 | Interactive tier composite request floor |
| `BLUNDER_EDITOR_VIEWPORT_IDLE_MS` | 100 | Idle tier composite request floor |
| `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_HOLD_MS` | 150 | Remain interactive after last input |
| `BLUNDER_EDITOR_VIEWPORT_PRESENT_MS` | 50 | Skia present floor while viewport updating |

#### Scenario: Override interactive interval

- **WHEN** `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_MS=40` is set before launch
- **THEN** interactive-tier composite requests SHALL be spaced by at least **40 ms**

