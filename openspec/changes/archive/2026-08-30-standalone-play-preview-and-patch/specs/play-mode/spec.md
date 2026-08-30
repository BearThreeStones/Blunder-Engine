## ADDED Requirements

### Requirement: Editor exposes Play Reload
The editor SHALL provide a dedicated Reload control on Play controls. Activating Reload while Playing or Paused SHALL run Play Reload on the existing Play Process. Save SHALL NOT run Play Reload. Starting Play while a session is already active SHALL still stop the existing session and spawn a new Play Process; that replacement SHALL NOT be Play Reload.

#### Scenario: Reload keeps the same process
- **WHEN** the author activates Reload while Playing and Reload preflight succeeds
- **THEN** the same Play Process reinstantiates the session’s captured On-disk Play entry scene
- **AND** the editor does not Stop and does not spawn a second Player

#### Scenario: Save does not Reload
- **WHEN** a Play session is Playing and the author Saves the Play entry scene
- **THEN** the Play Process world is unchanged by that Save
- **AND** Play Reload does not run

#### Scenario: Play while Playing is still Stop then spawn
- **WHEN** Play is requested while a Player is already running
- **THEN** the existing Player is stopped before a new Player is started
- **AND** that replacement is not Play Reload

### Requirement: Play entry is frozen at spawn
The Play entry scene for a session SHALL be the On-disk scene GUID/path captured when that Play Process was spawned. Opening a different scene in the editor during the session SHALL NOT retarget Play Reload or Play authorship patch to the newly opened scene.

#### Scenario: Switching the open scene does not retarget Reload
- **WHEN** a Play session was spawned on scene A and the author opens scene B then activates Reload
- **THEN** Reload instantiates the captured On-disk asset for scene A
- **AND** scene B is not the Reload source

### Requirement: Play Reload skips Scripts build
Play Reload SHALL NOT run Play Scripts build, SHALL NOT replace the Scripts assembly already loaded in that Play Process, and SHALL NOT run ALC. Behaviours SHALL remount from that already-loaded assembly. A Warning-grade Issue when Scripts are dirty SHALL NOT block Reload. New C# SHALL still require Stop then Play.

#### Scenario: Dirty Scripts warn and Reload proceeds
- **WHEN** Scripts are dirty and the author activates Reload with a valid saved Play entry
- **THEN** Reload proceeds without `dotnet build`
- **AND** a Warning-grade Issue is reported
- **AND** Behaviours remount from the already-loaded assembly

#### Scenario: New C# is not picked up by Reload
- **WHEN** the author adds a new C# Behaviour type, Saves, and Reloads without Stop then Play
- **THEN** the Play Process does not load a new Scripts assembly as part of Reload

### Requirement: Play Reload preserves Pause and still runs Ready
Play Reload SHALL NOT change the session’s Playing versus Paused flag. Ready SHALL run on remount even while Paused. Tick SHALL stay skipped while Paused.

#### Scenario: Reload while Paused stays Paused
- **WHEN** the session is Paused and Reload succeeds
- **THEN** the session remains Paused
- **AND** Ready has run on the remounted Behaviours
- **AND** Tick does not advance until Resume or Play step

### Requirement: Play Reload failure keeps the current world
If Play Reload camera preflight fails or the Player cannot instantiate the captured On-disk Play entry, the editor SHALL abort that Reload. The Play Process SHALL stay alive, the current gameplay world SHALL remain, and the session SHALL remain Playing or Paused. The editor SHALL NOT Stop the Player because Reload failed.

#### Scenario: Missing camera aborts Reload
- **WHEN** Reload is requested and Diagnose of the Play rule set reports Error-grade `play.missing_camera` on the entry as it would be loaded
- **THEN** Reload does not replace the Player world
- **AND** the Play Process is still running
- **AND** the session remains Playing or Paused

#### Scenario: Instantiate failure keeps the world
- **WHEN** Reload preflight succeeds and the Player fails to instantiate the captured On-disk Play entry
- **THEN** the current gameplay world remains
- **AND** the Play Process is still running

### Requirement: Play Reload does not replay Live Commands
After a successful Play Reload, the new Play Process world SHALL be the instantiated On-disk Play entry. The editor SHALL NOT replay sealed Live Commands onto that world as part of Reload. Subsequent Live seals SHALL use Play authorship patch.

#### Scenario: Unsaved seals are not on the reloaded world
- **WHEN** the author sealed Live Commands after the last Save, then Reloads using last saved
- **THEN** the Player world matches the last saved On-disk Play entry
- **AND** those unsaved sealed Commands are not replayed by Reload

### Requirement: Product Play is one Standalone Player
Product Play Mode SHALL be one Play Process (`engine_player`). The editor SHALL remain in Edit Mode during Play. The product SHALL NOT ship in-process PIE as Play, SHALL NOT Tick the authorship SceneInstance as Play, and SHALL NOT offer PIE and Standalone as two first-class Play stacks.

#### Scenario: Play stays a separate process
- **WHEN** the author activates Play and preflight succeeds
- **THEN** gameplay Tick runs in the Play Process
- **AND** the editor authorship SceneInstance is not the Play world

## MODIFIED Requirements

### Requirement: Editor exposes Play Pause and Stop
The editor SHALL provide Play, Pause, Stop, and Reload controls for the Play session. Play starts or resumes the session; Pause freezes gameplay Tick in the Player without ending the process; Stop ends the Play Process and returns the UI to Edit Mode (Stopped); Reload runs Play Reload on the existing Play Process.

#### Scenario: Play from Stopped
- **WHEN** the author activates Play while Stopped and preflight succeeds
- **THEN** the editor enters a Starting/Playing state and a Player process is spawned

#### Scenario: Pause while Playing
- **WHEN** the author activates Pause while Playing
- **THEN** the editor shows Paused and the Player stops advancing Behaviour Tick until Resume/Play

#### Scenario: Stop ends session
- **WHEN** the author activates Stop while Playing or Paused
- **THEN** the Play Process is requested to exit and the editor returns to Stopped

### Requirement: Headless Editor Play session
A Headless Editor SHALL run a Play session (spawn Player, pause, resume, stop, Play step, Play frame, Play Reload, Play authorship patch) on the Play control channel without UiHost or Slint Play controls. A Headless Editor SHALL spawn a Headless Player.

#### Scenario: Headless Play without editor chrome
- **WHEN** a Headless Editor starts Play and preflight succeeds
- **THEN** a Headless Player process is spawned
- **AND** the editor session becomes Starting/Playing without an editor OS window

#### Scenario: Headless Reload uses last saved
- **WHEN** a Headless Editor requests Play Reload while the Live document is dirty
- **THEN** no dirty-scene prompt is shown
- **AND** Reload instantiates the last saved captured Play entry asset

### Requirement: Dirty scene prompt before Play
When Play is requested on a windowed Editor and the active scene document is dirty, the editor SHALL prompt: save then Play, Play using the last saved asset, or cancel. When Play Reload is requested on a windowed Editor and the Live document that is this session’s Play entry scene (same Scene Asset GUID) is dirty, the editor SHALL prompt: save then Reload, Reload using the last saved asset, or cancel. Switching the open editor scene to a different document SHALL NOT make that other document the Reload dirty prompt. It SHALL NOT silently auto-save or silently proceed without indication when dirty. A Headless Editor SHALL NOT show that prompt; Play and Play Reload SHALL use the last saved Play entry asset (same as the windowed "play last saved" / "reload last saved" choice). Live Capture remains the Live document.

#### Scenario: Save and Play
- **WHEN** the author chooses Save and Play on a dirty scene
- **THEN** the scene is saved and Play proceeds using that saved asset

#### Scenario: Cancel dirty prompt
- **WHEN** the author chooses Cancel on the dirty prompt
- **THEN** Play does not start and the editor remains Stopped

#### Scenario: Headless Play uses last saved
- **WHEN** a Headless Editor starts Play while the Live document is dirty
- **THEN** no dirty-scene prompt is shown
- **AND** the Player loads the last saved Play entry asset

#### Scenario: Save then Reload
- **WHEN** a Play session is running, the Play entry Live document is dirty and still the open Live document, and the author chooses save then Reload
- **THEN** that Play entry is saved and Play Reload instantiates that saved On-disk Play entry

#### Scenario: Cancel dirty Reload prompt
- **WHEN** Reload is requested, the Play entry Live document is dirty, and the author chooses Cancel
- **THEN** Play Reload does not run
- **AND** the Play Process and current world remain

#### Scenario: Other open scene is not the Reload prompt
- **WHEN** a Play session was spawned on scene A, the author opens dirty scene B, and Reload is requested
- **THEN** the editor does not prompt to save scene B
- **AND** Reload instantiates the last saved captured Play entry for scene A

### Requirement: Edit Mode remains editable during Play
While a Play session is running, the author SHALL be able to continue editing the Project in the editor. Uncommitted Live edits SHALL NOT appear in the Player. Sealed Live Editor Commands MAY appear via Play authorship patch. The editor viewport MAY draw Play pose preview without changing Live. A newly saved Play entry scene SHALL replace the whole Player world only after Play Reload or a later Play (after save/build rules). Play Mode SHALL NOT lock the authorship document.

#### Scenario: Edit while Playing
- **WHEN** a Player is Playing and the author edits the active scene without sealing a v1-patchable Command
- **THEN** the editor accepts the edits and the running Player world is unchanged by those uncommitted edits

#### Scenario: Sealed Command may patch
- **WHEN** a Player is Playing and the author seals a v1-patchable Command on the Play entry Live document
- **THEN** the editor accepts the Command
- **AND** Play authorship patch may apply it to the Player

### Requirement: Play start camera gate
Play Mode start SHALL run Diagnose of the Play rule set for Camera on the Play entry scene after the scene path is known (as it will be loaded after dirty-prompt save rules) and before Player spawn. Play Reload SHALL run that same Diagnose on the captured Play entry as it will be loaded after dirty-prompt save rules and before the Player commits a new world. An Error-grade `play.missing_camera` Issue SHALL leave the editor in a non-Playing state for a spawn attempt (no Player process). On Reload, that Error SHALL abort Reload: the Play Process and current gameplay world SHALL stay, and the session SHALL remain Playing or Paused. The camera gate SHALL NOT be a separate stringly error type beside Issue.

#### Scenario: Preflight runs before spawn
- **WHEN** the user starts Play and the entry scene has no Camera
- **THEN** the engine SHALL report Error Issue `play.missing_camera` and SHALL NOT spawn Player

#### Scenario: Preflight runs before Reload commit
- **WHEN** the user Reloads and the captured Play entry as it would be loaded has no Camera
- **THEN** the engine SHALL report Error Issue `play.missing_camera`
- **AND** SHALL NOT replace the Player world
- **AND** the Play Process SHALL still be running

### Requirement: Clear on Play at session start
When Clear on Play is enabled, the editor SHALL perform Console clear when a Play session starts after successful preflight (as the Player is spawned). Play Reload SHALL NOT perform Console clear. When the toggle is off, existing rows SHALL remain.

#### Scenario: Toggle off keeps prior rows
- **WHEN** Clear on Play is off, the Console has rows, and Play starts
- **THEN** those rows remain visible alongside new Player messages

#### Scenario: Reload keeps Console rows
- **WHEN** Clear on Play is on, the Console has rows, and Play Reload succeeds
- **THEN** those rows remain

### Requirement: Play frame on the control channel
The Play control channel SHALL carry Play step, Play frame, Play Reload, and Play authorship patch in addition to pause, resume, and stop. Player → editor Play frames and Play pose preview SHALL use that same connection. The channel SHALL NOT add a second Play-session socket for frames, Reload, patches, or poses.

#### Scenario: Frame after step
- **WHEN** the session is Paused and Play step then Play frame run
- **THEN** the editor receives a 16:9 Play frame of the Play Process world

#### Scenario: One socket
- **WHEN** Play frame is requested
- **THEN** it uses the existing Play control channel

#### Scenario: Reload and patch share the socket
- **WHEN** Play Reload or a Play authorship patch is sent
- **THEN** it uses the existing Play control channel
- **AND** no second Play-session socket is opened
