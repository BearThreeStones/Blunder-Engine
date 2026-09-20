# Spec Delta

## Purpose

Gives a windowed Editor Session a readable open instrument on the Editor Shell after the Startup cover: a Godot red-box Editor modal with discrete percent, real boot captions, and elapsed seconds, dismissed when that session can Iterate.

## ADDED Requirements

### Requirement: Overlay after cover until Iterate
A windowed Editor Session SHALL show Editor open progress as a Slint Editor modal overlay on the Editor Shell after the Startup cover is gone. The overlay SHALL remain until Content Browser index refresh (the scan) has finished, the startup scene entity table is instantiated, and product Iterate can run. The overlay SHALL NOT wait for unique Mesh CPU+GPU residency, Texture Loader GPU copies, or Content Browser thumbnail 2/tick drain.

#### Scenario: Overlay covers the post-cover open gap
- **WHEN** a windowed Editor Session has presented the Editor Shell and startup scene instantiate or Content index refresh is still running
- **THEN** the Editor open progress overlay is visible on that Shell
- **AND** the Startup cover is not shown

#### Scenario: Overlay yields when Iterate can run
- **WHEN** Content index refresh has finished and the startup scene entity table exists
- **THEN** the overlay is gone
- **AND** product Iterate can run
- **AND** unique Mesh residency, texture copies, or thumbnail jobs MAY still be in progress

### Requirement: Godot red-box chrome on the Shell
The overlay SHALL be an authored Editor modal (14px radius, dim layer) centered on the already-visible Shell. It SHALL show title **Opening editor**, a percent bar, the current English stage caption, and elapsed whole seconds. It SHALL NOT be a second OS window. It SHALL NOT be a Player splash illustration. It SHALL NOT offer Cancel or Retry.

#### Scenario: Center modal on the Shell
- **WHEN** the overlay is visible
- **THEN** Application Bar, docks, or Viewport grid MAY already be on screen under the dim
- **AND** no second taskbar window is created for this progress
- **AND** the modal title is Opening editor

### Requirement: Discrete weighted percent, no fake crawl
Percent SHALL be the sum of fixed integer weights for completed named boot stages. Visible overlay captions SHALL be **Indexing content** (Content Browser index scan) and **Opening scene** (startup Scene Asset deserialize + instantiate). Cover stages **Cooking assets**, **Preparing editor**, and **Starting editor** SHALL count as already complete when the overlay appears, so the bar SHALL NOT restart those stages at 0%. Percent SHALL jump at stage boundaries. It SHALL NOT interpolate over wall time. Captions SHALL NOT copy Godot “global class names”.

#### Scenario: Overlay does not restart cook at zero
- **WHEN** the overlay first appears after the Startup cover
- **THEN** the percent already includes completed cover-stage weights
- **AND** the value is not 0% solely because cook already finished

#### Scenario: Percent jumps with the real stage
- **WHEN** Opening scene or Indexing content completes
- **THEN** the percent jumps to the new completed-weight total
- **AND** the caption matches the stage that is running or just completed
- **AND** the bar does not smoothly crawl between those jumps

### Requirement: Elapsed seconds from the first OS window
While the overlay is visible, it SHALL show elapsed whole seconds counted from the first OS window of this Editor Session (the same moment the Startup cover begins). The Startup cover SHALL NOT display that elapsed value.

#### Scenario: Seconds include cover time
- **WHEN** cook ran for several seconds on the Startup cover and the overlay then appears
- **THEN** the overlay elapsed seconds are already those cover seconds plus overlay time
- **AND** the count did not restart at overlay show

### Requirement: Pump and redraw while the overlay is up
While the overlay is visible, the session SHALL pump window events and Present the Shell plus overlay so close, resize, and progress redraw stay live. The percent bar SHALL NOT be a single frozen first Present for the whole remaining open.

#### Scenario: Close during overlay ends the session
- **WHEN** the user closes the session window while the overlay is visible
- **THEN** that Editor Session ends
- **AND** no Retry control is shown

#### Scenario: Overlay redraws during remaining open
- **WHEN** Opening scene or Indexing content runs long enough to observe
- **THEN** the overlay Present updates (elapsed and/or caption/percent)
- **AND** the OS window is not left without an event pump

### Requirement: Windowed engine_editor only
Editor open progress SHALL apply only to a windowed Editor Session (`engine_editor` with an OS window). Headless Editor (including CLI and MCP), Project Manager, and Player SHALL NOT show this overlay.

#### Scenario: Headless has no overlay
- **WHEN** an Editor Session starts Headless, or with CLI, or with MCP
- **THEN** Editor open progress is not shown

#### Scenario: Other windowed hosts stay without this overlay
- **WHEN** Project Manager or a windowed Player starts
- **THEN** Editor open progress is not shown

### Requirement: Session-start open only
This overlay SHALL cover the startup open of this Editor Session. Opening another Scene Asset from the Content Browser after product Iterate has begun SHALL NOT show this overlay.

#### Scenario: Later scene open has no overlay
- **WHEN** the author opens a different Scene Asset from the Content Browser after the session can Iterate
- **THEN** Editor open progress is not shown for that open

### Requirement: Fatal open is not an overlay error UI
If boot cannot reach product Iterate, the process SHALL end as it does today. The overlay SHALL NOT become an error page. It SHALL NOT offer Retry.

#### Scenario: Fatal failure during overlay
- **WHEN** startup scene load or Content index refresh fails fatally while the overlay is visible
- **THEN** the Editor Session process ends
- **AND** the overlay is not replaced by Retry or an error dialog

### Requirement: Instrument does not change loaders
This overlay SHALL NOT change cook, Mesh Loader tick budget, Texture Loader, or SE-world flatten. Waiting until Mesh / texture / thumbnail queues drain SHALL NOT be the dismiss gate.

#### Scenario: Forest may still grow after dismiss
- **WHEN** the overlay is gone and product Iterate is running
- **THEN** unique Mesh Assets MAY still become GPU-resident
- **AND** that in-flight residency SHALL NOT fail this capability
