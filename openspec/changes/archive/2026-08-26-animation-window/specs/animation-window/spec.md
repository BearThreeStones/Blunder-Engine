## Purpose

Persistent docked Edit animation preview on the current single selection's AnimationTree: transport, ruler clock, Fire slot, CINE session marks, and the shared TimeScale field — without overlay S0/S1 chrome or AnimationPlayer as the play host.

## ADDED Requirements

### Requirement: Persistent Animation dock
The Editor Session SHALL provide an **Animation Window** as its own dock panel kind, titled Animation, defaulting to a bottom dock under the viewport. Authors MAY retile or float it. The panel SHALL remain present when unbound. It SHALL NOT auto-hide or auto-show from Hierarchy selection. It SHALL NOT be Camera Preview and SHALL NOT host AnimationTree Canvas.

#### Scenario: Default layout places Animation under the viewport
- **WHEN** an Editor Session opens with the default dock layout
- **THEN** an Animation panel is docked below the Scene viewport

#### Scenario: Unbound selection keeps the dock open
- **WHEN** the Hierarchy selection is empty, multi-select, or a single entity with no AnimationTree
- **THEN** the Animation panel is still visible
- **AND** transport and the timeline are disabled

### Requirement: Bind to the selected AnimationTree
The window SHALL bind when Hierarchy has exactly one selected entity that has an AnimationTree. Changing selection SHALL Stop the previously bound tree (ruler to 0, clear Fire slot, halt transport, End CINE) then bind the new Tree or the empty state. An unbound previous tree SHALL NOT keep preview-advancing.

#### Scenario: Selecting Chocomel enables the window
- **WHEN** the author selects a single Object that has an AnimationTree
- **THEN** the Animation Window is enabled and bound to that tree

#### Scenario: Selecting Camera disables the window
- **WHEN** the bound tree is playing
- **AND** the author selects an entity with no AnimationTree
- **THEN** the previous tree is Stopped (ruler 0, Fire slot cleared, CINE ended)
- **AND** the window stays open and disabled

### Requirement: Ruler clock
The timeline SHALL be a ruler and playhead only (no bone/method track rows, no key diamonds). The ruler SHALL show the **base dominant clip** clock. While a Fire-slot or authored OneShot insert occupies, the ruler SHALL show that insert clip's clock. The readout SHALL show the clock source's logical name and current/length times.

#### Scenario: Idle base fills the ruler
- **WHEN** the bound tree's base dominant clip is Idle of length 2.0s and no insert occupies
- **THEN** the ruler length is 2.0s
- **AND** the clip name shown is Idle

#### Scenario: Fire switches the ruler to the insert
- **WHEN** Fire occupies the Fire slot with Attack of length 0.6s
- **THEN** the ruler length is 0.6s
- **AND** the clip name shown is Attack

### Requirement: Transport Play Pause Stop Loop
Play SHALL activate an inactive bound tree before advancing. Pause SHALL freeze the playhead. Stop SHALL seek the ruler to 0, clear the Fire slot, End CINE, and leave the tree active. Window Loop SHALL wrap the current ruler clip while Playing; it is session-only, not an AnimationClip Asset field and not AnimationPlayer loop. With Loop off, reaching the end SHALL Pause on the last frame and leave the tree active. Play from last-frame Pause SHALL start again at 0.

#### Scenario: Play activates an inactive tree
- **WHEN** the bound AnimationTree is inactive
- **AND** the author presses Play
- **THEN** the tree becomes active and preview advances

#### Scenario: Loop off pauses on the last frame
- **WHEN** Loop is off and Playing reaches the last frame of the current ruler clip
- **THEN** transport is Paused on that last frame
- **AND** the tree stays active

#### Scenario: Play after last-frame Pause starts at 0
- **WHEN** transport is Paused on the last frame
- **AND** the author presses Play
- **THEN** the playhead starts at 0

#### Scenario: Stop does not deactivate the tree
- **WHEN** the tree is active and Playing
- **AND** the author presses Stop
- **THEN** the playhead is at 0
- **AND** the Fire slot is empty
- **AND** CINE marks are cleared
- **AND** the tree stays active

### Requirement: Fire uses the bound tree Fire slot
Window Fire SHALL insert the Fire-target logical name on the bound tree's **Fire slot** (`RequestOneShot` semantics: hard-cut if occupied). The Fire-target control SHALL list that tree's existing Clip Binding logical names. It SHALL NOT retarget the playhead's base, Travel, or author bindings. Window Fire SHALL NOT be Sync Group `fireSameName`. v1 SHALL NOT show a Sync member list.

#### Scenario: Fire Attack inserts on the Fire slot
- **WHEN** the Fire target is Attack
- **AND** the author presses Fire
- **THEN** Attack occupies the bound tree Fire slot
- **AND** the ruler follows Attack

#### Scenario: Second Fire hard-cuts
- **WHEN** Attack occupies the Fire slot
- **AND** the author Fires Walk
- **THEN** Walk replaces Attack immediately
- **AND** Attack is not queued

### Requirement: CINE Enter and End
Enter SHALL set in-CINE and input-suppression session marks only; it SHALL NOT Fire a clip. End SHALL clear those marks without seeking and without clearing the Fire slot. Stop and rebind SHALL End CINE.

#### Scenario: Enter does not Fire
- **WHEN** the ruler is on Idle and the Fire slot is empty
- **AND** the author presses Enter
- **THEN** in-CINE and Inp badges are shown
- **AND** the Fire slot stays empty
- **AND** the playhead does not seek

#### Scenario: End leaves Fire occupancy
- **WHEN** CINE is on and Attack occupies the Fire slot
- **AND** the author presses End
- **THEN** in-CINE and Inp badges are hidden
- **AND** Attack still occupies the Fire slot

### Requirement: TimeScale is the tree field
Window TimeScale SHALL be the same AnimationTree playback-rate field the Inspector edits (dual-track). Committing it SHALL dirty the document. It SHALL be the only Animation Window control on Document History. Transport, playhead, Loop, Fire occupancy, and CINE marks SHALL NOT dirty the document.

#### Scenario: Slider matches Inspector
- **WHEN** the author sets window TimeScale to 1.5 and commits
- **THEN** Inspector AnimationTree TimeScale shows 1.5
- **AND** the document is dirty

#### Scenario: Play does not dirty
- **WHEN** the document is clean
- **AND** the author presses Play then Pause
- **THEN** the document stays clean

### Requirement: Icon-first chrome
Transport Play / Pause / Stop / Loop, Fire, and Enter / End CINE SHALL be **Editor Icon**-only with tooltip names (ADR 0042). The Fire-target dropdown SHALL keep Clip Binding logical names as text. CINE and Inp badges SHALL stay word badges. Dock tab title Animation MAY have a leading icon and SHALL keep the word Animation.

#### Scenario: Play is an icon
- **WHEN** the Animation Window is shown
- **THEN** the Play control shows the Play Editor Icon
- **AND** it has no visible word Play on the button face

### Requirement: Overlay preview toolbar is gone
The Scene viewport SHALL NOT show the overlay AnimationPreviewToolbar (Play/S0/S1/BW/Fd). The transform tool strip SHALL remain a viewport overlay.

#### Scenario: Viewport has transform strip only
- **WHEN** the Scene viewport is visible
- **THEN** Move / Rotate / Scale overlay the viewport
- **AND** no overlay S0 / S1 / BW / Fd toolbar is shown
