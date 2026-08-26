# host-observation Specification

## Purpose

Host observation so machines can take a 16:9 Scene still (Capture) and, during Play, step a paused Player then take a Play frame — without treating those stills as Authorship Query or Diagnose.

## Requirements

### Requirement: Host observation is not Authorship
Capture, Play step, and Play frame SHALL NOT be Query, Op, or Diagnose. They SHALL NOT push Editor History. They SHALL NOT be Console Messages.

#### Scenario: Capture does not push History
- **WHEN** Capture runs on the Live document
- **THEN** Document History is unchanged

#### Scenario: Play step is not an Op
- **WHEN** Play step advances the paused Play Process
- **THEN** no Editor Command is pushed

### Requirement: Capture is a 16:9 Scene still
Capture SHALL return a still from the Scene still path: Play-rule camera (Main Camera, else first valid Camera), no Editor Overlays, CPU readback. Aspect SHALL be 16:9 with a capped longest edge. Capture SHALL NOT write the Content Browser Thumbnail cache. Capture SHALL NOT use the OS or Slint window composite, the main viewport offscreen, or Camera Preview's selected camera.

#### Scenario: 16:9 not square
- **WHEN** Capture succeeds
- **THEN** the still width:height is 16:9
- **AND** the frame is not square

#### Scenario: No Camera is failure
- **WHEN** Capture targets a scene with no valid Camera
- **THEN** Capture fails
- **AND** no Editor Camera fallback still is produced

#### Scenario: Thumbnail cache unchanged
- **WHEN** Capture runs
- **THEN** the Content Browser Thumbnail cache is not written as that Capture

### Requirement: Capture Subject
Live Capture SHALL render the Live document SceneInstance (including unsaved edits). On-disk Capture SHALL instantiate the named Scene Asset as Scene Thumbnail Render does, not the dirty Live document.

#### Scenario: Live sees unsaved Op
- **WHEN** the Live document has unsaved translation on Player and Capture is Live
- **THEN** the still is of that unsaved pose

#### Scenario: On-disk ignores unsaved
- **WHEN** the Live document is dirty and Capture is On-disk for the saved Scene Asset
- **THEN** the still is of the saved asset, not the unsaved Live edits

### Requirement: Scene Thumbnail stays square
Scene Thumbnail Render SHALL keep square aspect and SHALL keep writing the Content Browser Thumbnail cache. The shared Scene still path SHALL take aspect as a parameter.

#### Scenario: Thumb remains square
- **WHEN** a Scene Thumbnail is generated
- **THEN** the cached still is square

### Requirement: Play frame is not Capture
A Play frame SHALL be a still of the Play Process world through the Play-rule camera (gameplay already ticked or paused). It SHALL NOT be a Scene still and SHALL NOT be called Capture. Product aspect SHALL match Capture (16:9, capped longest edge). It SHALL ride the Play control channel. It SHALL NOT scrape the Player OS window.

#### Scenario: Distinct worlds
- **WHEN** the Live document was edited during Play and a Play frame is taken
- **THEN** the Play frame shows the Play Process world, not those unsaved editor edits

### Requirement: Play dump is out
Host observation v1 SHALL NOT include a structured Play dump of Object properties.

#### Scenario: No dump API
- **WHEN** a client completes Play step and Play frame
- **THEN** the contract does not require a JSON world dump in this slice

### Requirement: Headless uses the same observation
Headless Editor and Headless Player SHALL use Capture, Play step, and Play frame as specified for Host observation. They SHALL NOT introduce a second observation protocol. Capture SHALL NOT require Slint or an OS window.

#### Scenario: Headless Capture without a window
- **WHEN** Capture runs on a Headless Editor
- **THEN** the still is a 16:9 Scene still from the Scene still path
- **AND** no OS window or Slint composite is used

#### Scenario: Headless Play frame without a window
- **WHEN** a Headless Player is Paused and Play step then Play frame run
- **THEN** the editor receives a 16:9 Play frame of the Play Process world
- **AND** the frame is not an HWND scrape

### Requirement: Adapters emit stills as PNG
CLI Capture and CLI play-frame SHALL write PNG to `--out`. MCP Capture and MCP play-frame SHALL return PNG ImageContent. They SHALL NOT scrape HWND. They SHALL NOT return stills as Query or Diagnose.

#### Scenario: CLI Capture writes PNG
- **WHEN** CLI Capture succeeds with `--out`
- **THEN** `--out` is a PNG of the 16:9 Scene still
- **AND** stdout JSON does not contain the image bytes

### Requirement: CLI play-frame is the episode presentation
CLI play-frame SHALL produce one Play frame by composing the existing Play Session (start, optional Pause and Play step, one Play frame, Stop) in one process. MCP play-frame SHALL remain a Play frame request on an existing session. Both SHALL use the name play-frame. The CLI episode SHALL NOT be a second observation protocol and SHALL NOT be named capture-play.

#### Scenario: MCP play-frame needs an active session
- **WHEN** MCP play-frame is called while Play is Stopped
- **THEN** the result is Request failure
- **AND** no PNG ImageContent is returned
