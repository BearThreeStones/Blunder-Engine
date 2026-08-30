## Purpose

Draws Play Process entity poses in the editor viewport for Play-entry entities so authors can see Tick-driven motion without writing Live.

## ADDED Requirements

### Requirement: Editor viewport draws Play Process poses
While a Play session is Playing or Paused, the editor viewport SHALL draw Play pose preview for entities that exist in this session’s Play entry scene, keyed by Authorship Address. Runtime-spawned Player entities with no Authorship Address SHALL NOT appear as Play pose preview.

#### Scenario: Named Play-entry entity is previewed
- **WHEN** a Play session is Playing and the Play Process world has an entity named `Hero` that is also in the Play entry scene
- **THEN** the editor viewport draws that entity using the Play Process pose for Authorship Address `Hero`

#### Scenario: Unnamed runtime spawn is not previewed
- **WHEN** a Behaviour in the Player spawns an entity with no Authorship Address
- **THEN** the editor viewport does not draw that entity as Play pose preview

### Requirement: Preview does not write Live
Play pose preview SHALL NOT write the Live document, SHALL NOT dirty authorship, SHALL NOT push Document History, and SHALL NOT change Inspector values or Transform gizmo handles. Inspector and gizmo handles SHALL remain on Live authorship.

#### Scenario: Tick motion leaves Inspector authored
- **WHEN** Play pose preview shows a moving entity in the editor viewport
- **THEN** the Inspector Local Transform for that entity remains the Live authored values
- **AND** Document History has no new Command from the preview

#### Scenario: Gizmo handle stays on Live
- **WHEN** Play pose preview is drawing and the author selects that entity
- **THEN** Transform gizmo handles sit on the Live authored pose, not the Play Process pose

### Requirement: Preview is not Capture Play frame or Query
Play pose preview SHALL ride the existing Play control channel. It SHALL NOT be Capture, SHALL NOT be a Play frame still, SHALL NOT be Query of the Play Process as an Authorship Subject, and SHALL NOT open a second Play-session socket.

#### Scenario: Poses on the control channel
- **WHEN** a Play session is Playing
- **THEN** the Player sends pose records on the same Play control channel used for pause, logs, and Play frames
- **AND** no second Play-session socket is opened for poses

#### Scenario: Play frame is still a still
- **WHEN** the author requests a Play frame while pose preview is active
- **THEN** Play frame remains a 16:9 still of the Play Process world
- **AND** that still is not the editor viewport pose preview

### Requirement: Stop and Reload end the current preview stream
Stop SHALL end Play pose preview. Play Reload SHALL end the current preview stream; after a successful Reload the editor SHALL preview the new Play Process world.

#### Scenario: Stop clears preview
- **WHEN** the author Stops Play while pose preview is drawing
- **THEN** the editor viewport no longer draws Play Process poses

#### Scenario: Reload follows the new world
- **WHEN** Play Reload succeeds
- **THEN** the previous preview stream ends
- **AND** subsequent preview draws poses from the reinstantiated Play Process world
