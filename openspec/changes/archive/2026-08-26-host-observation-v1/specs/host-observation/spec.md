## Purpose

Host observation so machines can take a 16:9 Scene still (Capture) and, during Play, step a paused Player then take a Play frame — without treating those stills as Authorship Query or Diagnose.

## ADDED Requirements

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
