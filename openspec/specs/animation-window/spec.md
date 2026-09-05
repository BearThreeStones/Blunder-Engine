# animation-window Specification

## Purpose

Persistent docked Edit animation preview on the current single selection's AnimationTree: transport, ruler clock, **Clip anatomy**, **Preview clip**, Fire slot, CINE session marks, and the shared TimeScale field — without overlay S0/S1 chrome or AnimationPlayer as the play host.

## Requirements

### Requirement: Persistent Animation dock
The Editor Session SHALL provide an **Animation Window** as its own dock panel kind, titled Animation, defaulting to a bottom dock under the viewport. Authors MAY retile or float it. The panel SHALL remain present when unbound. It SHALL NOT auto-hide or auto-show from Hierarchy selection. It SHALL NOT be Camera Preview and SHALL NOT host AnimationTree Canvas.

#### Scenario: Default layout places Animation under the viewport
- **WHEN** an Editor Session opens with the default dock layout
- **THEN** an Animation panel is docked below the Scene viewport

#### Scenario: Unbound selection keeps the dock open
- **WHEN** the Hierarchy selection is empty, multi-select, or a single entity with no AnimationTree
- **THEN** the Animation panel is still visible
- **AND** transport and the timeline are disabled
- **AND** Clip anatomy is not shown

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

### Requirement: Ruler clock
The timeline SHALL show the ruler clock plus **Clip anatomy** of the current ruler clip. The ruler SHALL show the **base dominant clip** clock. While a Fire-slot or authored OneShot insert occupies, the ruler SHALL show that insert clip's clock. While a Clip Play override is set and no Fire/OneShot insert occupies, the ruler SHALL show the Clip Play clip's clock. The readout SHALL show the clock source's logical name and current/length times. **Method tracks** SHALL NOT appear in Clip anatomy.

#### Scenario: Idle base fills the ruler
- **WHEN** the bound tree's base dominant clip is Idle of length 2.0s and no insert occupies
- **THEN** the ruler length is 2.0s
- **AND** the clip name shown is Idle

#### Scenario: Fire switches the ruler to the insert
- **WHEN** Fire occupies the Fire slot with Attack of length 0.6s
- **THEN** the ruler length is 0.6s
- **AND** the clip name shown is Attack

#### Scenario: Clip Play override fills the ruler
- **WHEN** Clip Play override is Hit of length 0.5s and the Fire slot is empty
- **THEN** the ruler length is 0.5s
- **AND** the clip name shown is Hit

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

### Requirement: CINE Enter and End
Enter SHALL set in-CINE and input-suppression session marks only; it SHALL NOT Fire a clip. End SHALL clear those marks without seeking and without clearing the Fire slot or Clip Play override. Stop and rebind SHALL End CINE.

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

#### Scenario: Filter and fold do not dirty
- **WHEN** the document is clean
- **AND** the author filters bone groups by name and collapses a bone group
- **THEN** the document stays clean

### Requirement: Icon-first chrome
Transport Play / Pause / Stop / Loop, Fire, and Enter / End CINE SHALL be **Editor Icon**-only with tooltip names (ADR 0042). The Preview clip dropdown SHALL keep Clip Binding logical names as text. CINE and Inp badges SHALL stay word badges. Dock tab title Animation MAY have a leading icon and SHALL keep the word Animation.

#### Scenario: Play is an icon
- **WHEN** the Animation Window is shown
- **THEN** the Play control shows the Play Editor Icon
- **AND** it has no visible word Play on the button face

### Requirement: Clip anatomy lists existing clip tracks
When the Animation Window is bound, it SHALL list the **clip tracks that already exist** on the current ruler **AnimationClip**. A clip track is one bone plus one of Position, Rotation, or Scale. Missing TRS channels SHALL be omitted (no Skeleton × Position/Rotation/Scale fill). **Method tracks** SHALL NOT be listed. Clip anatomy SHALL follow the same ruler clip as the clock (base dominant, Fire/OneShot insert, or Clip Play override).

#### Scenario: Existing channels only
- **WHEN** the ruler clip has empty_parent-R Position and Rotation keys and no Scale channel
- **THEN** that bone group shows Position and Rotation rows
- **AND** no Scale row is shown for that bone

#### Scenario: Fire switches anatomy
- **WHEN** the ruler clip is Idle
- **AND** the author Fires Attack
- **THEN** Clip anatomy lists Attack's clip tracks
- **AND** Idle's tracks are not shown

#### Scenario: Unbound hides anatomy
- **WHEN** the author selects an entity with no AnimationTree
- **THEN** Clip anatomy is not shown

### Requirement: Clip anatomy grouping and order
Clip anatomy rows SHALL be grouped by bone name. Bone groups SHALL follow the bone's first appearance in the clip. Within a group the channel order SHALL be Position, then Rotation, then Scale, omitting channels the clip does not have. The listing SHALL NOT use a Skeleton3D or scene-tree root as the group parent. The listing SHALL NOT reorder groups by Skeleton hierarchy or alphabetically.

#### Scenario: First appearance then TRS
- **WHEN** the clip stores Scale for bone B, then Position and Rotation for bone A, then Position for bone B
- **THEN** group B appears before group A
- **AND** group B lists Position then Scale (no Rotation row)
- **AND** group A lists Position then Rotation

### Requirement: Channel row chrome
Each channel row SHALL show the Position / Rotation / Scale **Editor Icon** plus that word, matching **Local Transform** labels. Bone group titles SHALL be the bone name only (no Skeleton glyph).

#### Scenario: Position row has icon and word
- **WHEN** Clip anatomy shows a Position clip track
- **THEN** the row shows the Position Editor Icon
- **AND** the row shows the word Position

### Requirement: Key diamonds are display-only
Each clip-track lane SHALL draw a diamond at each of that track's key times. Diamonds SHALL NOT be selectable or draggable.

#### Scenario: Diamonds mark key times
- **WHEN** a Position track has keys at 0.0s and 0.5s on a 1.0s clip
- **THEN** that lane shows two diamonds at those times

#### Scenario: Diamond is not a selection
- **WHEN** the author clicks a diamond
- **THEN** no key is selected
- **AND** the diamond cannot be dragged to another time

### Requirement: Playhead spans visible clip-track rows
The playhead SHALL be one vertical line through the ruler and the currently visible clip-track rows.

#### Scenario: Playhead crosses anatomy
- **WHEN** Clip anatomy shows three channel rows and the playhead is at 0.5s of a 1.0s clip
- **THEN** a single vertical playhead crosses the ruler and those three rows at 0.5s

### Requirement: Seek is timeline-column only
Press or drag on the timeline column (ruler and key lanes) SHALL seek with the existing ruler rule. Press on the name, filter, or fold column SHALL NOT seek. Diamonds SHALL NOT be a separate hit target (a press on a diamond is a press on the timeline column).

#### Scenario: Drag on key lane seeks
- **WHEN** the author drags horizontally in a clip-track key lane
- **THEN** the playhead seeks as it does on the ruler

#### Scenario: Name column does not seek
- **WHEN** the author presses a bone group title or a channel row name
- **THEN** the playhead does not seek

### Requirement: Session bone-name filter
The Animation Window SHALL provide a session filter that hides bone groups whose names do not contain the query (substring). It SHALL NOT filter by the channel words Position, Rotation, or Scale as the query target. It SHALL NOT persist on the Scene or the AnimationClip. It SHALL NOT dirty the document.

#### Scenario: Filter hides unmatched groups
- **WHEN** Clip anatomy has groups empty_parent-R and empty_parent-L
- **AND** the author types parent-R in the filter
- **THEN** empty_parent-R remains
- **AND** empty_parent-L is hidden

### Requirement: Session bone-group fold
Bone groups SHALL be collapsible and default expanded. Fold state SHALL live in this Editor Session only. Fold SHALL NOT persist on the Scene or the AnimationClip. When the ruler clip changes, every bone group SHALL re-expand.

#### Scenario: Collapse stays until clip change
- **WHEN** the author collapses bone group empty_parent-R
- **THEN** that group's channel rows are hidden
- **AND** the document is not dirty

#### Scenario: Ruler clip change expands groups
- **WHEN** empty_parent-R is collapsed
- **AND** the ruler clip changes from Idle to Attack
- **THEN** all bone groups are expanded

### Requirement: Unmatched Skeleton bones still listed
Clip-track bones that are not on the bound **Skeleton** SHALL still form groups. This pass SHALL NOT badge them and SHALL NOT hide them.

#### Scenario: Extra clip bone is listed
- **WHEN** the ruler clip has a clip track for bone helper_R
- **AND** the bound Skeleton has no bone named helper_R
- **THEN** helper_R still appears as a bone group
- **AND** no warning badge is shown

### Requirement: Overlay preview toolbar is gone
The Scene viewport SHALL NOT show the overlay AnimationPreviewToolbar (Play/S0/S1/BW/Fd). The transform tool strip SHALL remain a viewport overlay.

#### Scenario: Viewport has transform strip only
- **WHEN** the Scene viewport is visible
- **THEN** Move / Rotate / Scale overlay the viewport
- **AND** no overlay S0 / S1 / BW / Fd toolbar is shown
