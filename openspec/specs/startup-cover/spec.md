# startup-cover Specification

## Purpose
Gives a windowed Editor Session a branded Startup cover on its OS window from first show until the Editor Shell is on screen, so boot is not a black HWND or a silent wait.
## Requirements
### Requirement: Cover on the session window until Shell
A windowed Editor Session SHALL show a Startup cover on the same OS window that hosts the Editor Shell. That window SHALL appear after the Project File can yield the Project display name and still before cook and Vulkan. The cover SHALL remain until the Editor Shell is on screen. The cover SHALL NOT wait for the Live scene or the first 3D viewport frame. There SHALL be no minimum on-screen time: when the Shell is on screen, the cover is gone.

#### Scenario: First window is the cover
- **WHEN** a windowed Editor Session starts for a Project with a readable Project File
- **THEN** the first visible client of that session window is the Startup cover
- **AND** that window is the same window that later shows the Editor Shell
- **AND** cook has not finished before that window is shown

#### Scenario: Cover yields to Shell
- **WHEN** the Editor Shell is on screen
- **THEN** the Startup cover is gone
- **AND** the session does not hold the cover for an extra dwell

#### Scenario: Empty viewport is not the cover
- **WHEN** the Editor Shell is on screen and the Viewport has not shown a 3D frame yet
- **THEN** the Startup cover is not shown
- **AND** the empty Viewport is the Shell, not the cover

### Requirement: Brand field
The Startup cover SHALL fill the window client with Editor Theme Window (Base 2). It SHALL show the Application Bar wordmark centered (`Blunder Editor - {Project display name}`) and a short English stage name under it. Visible stage names SHALL be **Cooking assets**, **Preparing editor**, and **Starting editor**, and SHALL change as those phases run. The cover SHALL NOT show a percentage, a splash illustration, or a Logo image. The wordmark SHALL NOT include a Scene name or a dirty `*`.

#### Scenario: Wordmark and theme
- **WHEN** the Startup cover is visible
- **THEN** the client fill is Editor Theme Window
- **AND** the centered wordmark is `Blunder Editor - {Project display name}`
- **AND** a short English stage name is visible under the wordmark

#### Scenario: Stage names track boot
- **WHEN** cook or other boot work runs long enough to observe more than one phase
- **THEN** the stage name changes among Cooking assets, Preparing editor, and Starting editor
- **AND** no percent value is shown

### Requirement: Normal window while covering
While the Startup cover is visible, the session window SHALL remain a normal OS window. Close SHALL abort boot and end that Editor Session with no confirm dialog. Minimize and resize SHALL remain available. The cover SHALL fill the client after a resize. The session SHALL pump window events during remaining boot so close, resize, and cover redraw stay live.

#### Scenario: Close aborts boot
- **WHEN** the user closes the session window while the Startup cover is visible
- **THEN** that Editor Session ends
- **AND** no confirm dialog is shown

#### Scenario: Resize keeps the cover
- **WHEN** the user resizes the session window while the Startup cover is visible
- **THEN** the cover still fills the client
- **AND** boot continues until Shell or close

### Requirement: Windowed Editor only
The Startup cover SHALL apply only to a windowed Editor Session. Headless Editor (including CLI and MCP), Project Manager, and Player SHALL NOT show a Startup cover.

#### Scenario: Headless has no cover
- **WHEN** an Editor Session starts Headless, or with CLI, or with MCP
- **THEN** no OS window is created for a Startup cover
- **AND** no Startup cover is shown

#### Scenario: Other windowed hosts stay uncovered this cut
- **WHEN** Project Manager or a windowed Player starts
- **THEN** no Startup cover is shown

### Requirement: Fatal boot is not a cover error UI
If boot cannot reach the Editor Shell, the process SHALL end as it does today. The Startup cover SHALL NOT become an error page. It SHALL NOT offer Retry. It SHALL NOT show an error dialog on the cover. The last stage name MAY remain visible until the process exits.

#### Scenario: Fatal failure during cover
- **WHEN** boot fails fatally while the Startup cover is visible
- **THEN** the Editor Session process ends
- **AND** the cover is not replaced by an error page, Retry control, or error dialog
