## MODIFIED Requirements

### Requirement: Bind to the selected AnimationTree
The window SHALL bind when Hierarchy has exactly one selected entity that has an AnimationTree. Changing selection SHALL end preview on the previously bound tree (ruler to 0, clear Fire slot, clear Clip Play override, halt transport, End CINE) then bind the new Tree or the empty state. An unbound previous tree SHALL NOT keep preview-advancing. Binding a Tree SHALL set **Preview clip** to that tree's default Clip Binding logical name, activate the tree if inactive, **Clip Play** that name at clock 0, and SHALL NOT start transport.

#### Scenario: Selecting Chocomel enables the window
- **WHEN** the author selects a single Object that has an AnimationTree whose default clip is walk
- **THEN** the Animation Window is enabled and bound to that tree
- **AND** Preview clip is walk
- **AND** Clip Play override is walk at clock 0
- **AND** transport is not Playing

#### Scenario: Selecting Camera disables the window
- **WHEN** the bound tree is playing
- **AND** the author selects an entity with no AnimationTree
- **THEN** the previous tree's preview is ended (ruler 0, Fire slot cleared, Clip Play override cleared, CINE ended)
- **AND** the window stays open and disabled

### Requirement: Transport Play Pause Stop Loop
Play SHALL activate an inactive bound tree before advancing, then **Clip Play** **Preview clip**. Pause SHALL freeze the playhead. Stop SHALL seek the ruler to 0, clear the Fire slot, End CINE, and leave the tree active. Stop SHALL NOT clear Clip Play override. Window Loop SHALL wrap the current ruler clip while Playing; it is session-only, not an AnimationClip Asset field and not AnimationPlayer loop. With Loop off, reaching the end SHALL Pause on the last frame and leave the tree active. Play from last-frame Pause SHALL start again at 0.

#### Scenario: Play activates an inactive tree
- **WHEN** the bound AnimationTree is inactive
- **AND** the author presses Play
- **THEN** the tree becomes active and preview advances

#### Scenario: Play Clip Plays Preview clip
- **WHEN** Preview clip is idle
- **AND** the tree's default clip is walk
- **AND** the author presses Play
- **THEN** Clip Play override is idle
- **AND** the ruler follows idle

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
- **AND** Clip Play override is idle
- **AND** the author presses Stop
- **THEN** the playhead is at 0
- **AND** the Fire slot is empty
- **AND** Clip Play override is still idle
- **AND** CINE marks are cleared
- **AND** the tree stays active

### Requirement: Fire uses the bound tree Fire slot
Window Fire SHALL insert **Preview clip** on the bound tree's **Fire slot** (`RequestOneShot` semantics: hard-cut if occupied). The Preview clip control SHALL list that tree's existing Clip Binding logical names. Changing Preview clip SHALL **Clip Play** the new name immediately (hard cut, clock 0) whether Playing, Paused, or Stopped. Playing SHALL continue; Paused and Stopped SHALL NOT start transport. If the Fire slot occupies, changing Preview clip SHALL NOT clear the insert; the ruler SHALL stay on the insert until Fire ends. It SHALL NOT Travel or author bindings. Window Fire SHALL NOT be Sync Group `fireSameName`. v1 SHALL NOT show a Sync member list. v1 SHALL NOT add a second Clip Play dropdown beside Fire.

#### Scenario: Fire Attack inserts on the Fire slot
- **WHEN** Preview clip is Attack
- **AND** the author presses Fire
- **THEN** Attack occupies the bound tree Fire slot
- **AND** the ruler follows Attack

#### Scenario: Second Fire hard-cuts
- **WHEN** Attack occupies the Fire slot
- **AND** Preview clip is Walk
- **AND** the author Fires
- **THEN** Walk replaces Attack immediately
- **AND** Attack is not queued

#### Scenario: Changing Preview clip while Playing hard-cuts
- **WHEN** transport is Playing and Preview clip is walk
- **AND** the author sets Preview clip to idle
- **THEN** Clip Play override is idle at clock 0
- **AND** transport stays Playing
- **AND** the ruler follows idle

#### Scenario: Changing Preview clip while Stopped does not start transport
- **WHEN** transport is Stopped
- **AND** the author sets Preview clip to idle
- **THEN** Clip Play override is idle at clock 0
- **AND** transport stays Stopped

#### Scenario: Changing Preview clip while Fire occupies keeps the insert
- **WHEN** idle occupies the Fire slot
- **AND** the author sets Preview clip to walk
- **THEN** idle still occupies the Fire slot
- **AND** Clip Play override is walk
- **AND** the ruler still follows idle

### Requirement: TimeScale is the tree field
Window TimeScale SHALL be the same AnimationTree playback-rate field the Inspector edits (dual-track). Committing it SHALL dirty the document. It SHALL be the only Animation Window control on Document History. Transport, playhead, Loop, Fire occupancy, Clip Play override, CINE marks, Preview clip, Clip anatomy filter, and bone-group fold SHALL NOT dirty the document.

#### Scenario: Slider matches Inspector
- **WHEN** the author sets window TimeScale to 1.5 and commits
- **THEN** Inspector AnimationTree TimeScale shows 1.5
- **AND** the document is dirty

#### Scenario: Play does not dirty
- **WHEN** the document is clean
- **AND** the author presses Play then Pause
- **THEN** the document stays clean

#### Scenario: Preview clip does not dirty
- **WHEN** the document is clean
- **AND** the author changes Preview clip
- **THEN** the document stays clean

### Requirement: Icon-first chrome
Transport Play / Pause / Stop / Loop, Fire, and Enter / End CINE SHALL be **Editor Icon**-only with tooltip names (ADR 0042). The Preview clip dropdown SHALL keep Clip Binding logical names as text. CINE and Inp badges SHALL stay word badges. Dock tab title Animation MAY have a leading icon and SHALL keep the word Animation.

#### Scenario: Play is an icon
- **WHEN** the Animation Window is shown
- **THEN** the Play control shows the Play Editor Icon
- **AND** it has no visible word Play on the button face
