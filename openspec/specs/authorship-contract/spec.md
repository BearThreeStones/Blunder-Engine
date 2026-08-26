# authorship-contract Specification

## Purpose

First-party Query / Op / Diagnose contract so authors and machines share History, scene-unique names, and structured Issues without a Command Seam or a second mutation path.

## Requirements

### Requirement: Three intents
The Authorship contract SHALL expose Query, Op, and Diagnose. Query and Diagnose SHALL NOT push Editor History. One successful Op SHALL commit exactly one Editor Command on Document History or Global History. Diagnose SHALL NOT be a Query projection. The contract SHALL NOT be a Seam and SHALL NOT be Message.

#### Scenario: Transform Op pushes one Command
- **WHEN** a transform Op succeeds on a named entity in the Live document
- **THEN** Document History gains exactly one transform Command
- **AND** undo restores the previous local transform

#### Scenario: Query does not push History
- **WHEN** a Query lists entity names on the Live document
- **THEN** Document History is unchanged

#### Scenario: Diagnose does not push History
- **WHEN** Diagnose runs the Play rule set
- **THEN** Document History is unchanged

### Requirement: Authorship Address
Query, Op, and Diagnose SHALL address a scene entity by that entity's scene-unique name. They SHALL NOT use EntityId or ObjectId as the public target in this slice. Tombstoned entities and empty names SHALL NOT be valid scene addresses.

#### Scenario: Get by name
- **WHEN** the Live document has an editable entity named Player
- **THEN** a Query for Player returns that entity's name, parent name, and local TRS

#### Scenario: Unknown name is Request failure
- **WHEN** a Query or Op targets a name that is missing, empty, or tombstoned
- **THEN** the result is Request failure `address.unknown`
- **AND** no Editor Command is pushed
- **AND** the result is not an Issue list

### Requirement: Authorship Subject
Every Query and Diagnose SHALL state whether it evaluates the Live document or the On-disk Project. Op SHALL always target the Live document. On-disk Query and Diagnose SHALL name the Scene Asset by the same virtual path Play uses for the Play entry. The Player simulation SHALL NOT be an Authorship Subject.

#### Scenario: Op refuses On-disk Project
- **WHEN** an Op is requested against the On-disk Project
- **THEN** the result is Request failure `subject.live_required`
- **AND** no Editor Command is pushed

#### Scenario: Live Query without an open document
- **WHEN** a Live Query is requested and no Live document is open
- **THEN** the result is Request failure `subject.no_live_document`

#### Scenario: On-disk scene missing
- **WHEN** an On-disk Query or Diagnose names a Scene Asset that cannot be read
- **THEN** the result is Request failure `subject.scene_unreadable`

### Requirement: Query v1
A successful Query SHALL return either the list of editable scene entity names or one entity: name, parent name (empty when the entity is a root), local translation, local rotation as a quaternion, and local scale. The name list SHALL omit tombstoned entities and empty names.

#### Scenario: Name list omits tombstones
- **WHEN** the Live document has Cube editable and Sphere tombstoned
- **THEN** the name-list Query includes Cube and does not include Sphere

#### Scenario: Root parent name is empty
- **WHEN** a Query reads a root entity named Empty
- **THEN** that entity's parent name is empty

#### Scenario: On-disk Query reads saved TRS
- **WHEN** an On-disk Query reads a saved Scene Asset that has Player at local translation (1, 0, 0)
- **THEN** the Query for Player returns translation (1, 0, 0)
- **AND** unsaved Live document edits are not included

### Requirement: Transform Op v1
A transform Op SHALL set the target entity's full local TRS (translation, quaternion rotation, scale) on the Live document. On success it SHALL apply the edit and push exactly one existing transform Editor Command. Rotation SHALL be quaternion-authoritative.

#### Scenario: Set translation
- **WHEN** the Live document has Player at the origin and a transform Op sets translation to (1, 0, 0) with unchanged rotation and scale
- **THEN** Player's local translation is (1, 0, 0)
- **AND** Document History can undo back to the origin

### Requirement: Diagnose returns Issues
A successful Diagnose SHALL return a list of Issues. Each Issue SHALL have a stable code, an Issue severity of Log, Warning, or Error (the same three names as Console severity), an optional Authorship Address, and an explanation. Issue SHALL NOT be a Console Message. Request failure SHALL NOT be an Issue.

#### Scenario: Empty list when Play rules pass
- **WHEN** Diagnose runs the Play rule set on a subject that has a Camera and Scripts are not dirty and a scripts output exists or the Project has no Scripts
- **THEN** the Issue list is empty

### Requirement: Play rule set
Diagnose of the Play rule set SHALL emit `play.missing_camera` as Error when the subject scene has no valid Camera. It SHALL emit `scripts.dirty` as Warning when Scripts sources are newer than the last successful scripts output. It SHALL emit `scripts.missing_output` as Warning when Scripts exist but no game assembly output is present. Diagnose SHALL NOT compile Scripts and SHALL NOT Cook. Missing Camera SHALL NOT auto-inject a Camera.

#### Scenario: Missing Camera is Error
- **WHEN** Diagnose runs the Play rule set on a scene with no Camera
- **THEN** the list contains an Error Issue with code `play.missing_camera`
- **AND** no Camera is created

#### Scenario: Dirty Scripts are Warning without compile
- **WHEN** Diagnose runs the Play rule set and Scripts sources are newer than scripts output
- **THEN** the list contains a Warning Issue with code `scripts.dirty`
- **AND** Diagnose does not invoke a Scripts build

#### Scenario: Missing scripts output is Warning
- **WHEN** Diagnose runs the Play rule set on a Project that has Scripts but no game assembly under `.blunder/scripts_bin`
- **THEN** the list contains a Warning Issue with code `scripts.missing_output`
- **AND** Diagnose does not invoke a Scripts build

### Requirement: Authorship System is editor-only
An Editor Session SHALL mount the Authorship System as a Registered System. The Player host SHALL NOT mount it. On-disk Query and Diagnose SHALL be callable without the Authorship System and without the Player.

#### Scenario: Player has no Authorship System
- **WHEN** a process starts as the Player host
- **THEN** the Authorship System is not created

#### Scenario: On-disk Diagnose without the System
- **WHEN** a test or tool Diagnoses an On-disk Project Play rule set
- **THEN** it receives Issues without mounting the Authorship System in a Player

### Requirement: CLI and MCP adapt Authorship
CLI and MCP SHALL adapt Query, Op, and Diagnose from the Editor Session process. They SHALL NOT expose this contract from the Player. They SHALL NOT merge stills into Query, Op, or Diagnose. They SHALL share one verb set with Host observation and Play Session control — two presentations, not two catalogs.

#### Scenario: Query via CLI does not Capture
- **WHEN** CLI Query lists entity names on a Live document
- **THEN** the stdout JSON contains names
- **AND** no PNG is produced

### Requirement: Adapter Live requires --scene
A CLI or MCP Editor Session SHALL have a Live document only when launched with `--scene`. Without `--scene`, Live Query, Live Diagnose, and Op SHALL fail as Request failure `subject.no_live_document`. On-disk Query and Diagnose SHALL still name a Scene Asset virtual path.

#### Scenario: Op without --scene
- **WHEN** CLI Op is requested and the launch omitted `--scene`
- **THEN** the result is Request failure `subject.no_live_document`
- **AND** no Editor Command is pushed

### Requirement: Persist is not an Op
MCP Save SHALL persist the Live document through the editor save path and SHALL NOT be an Op. CLI Op SHALL require `--save` to persist after the Command; without it the Op SHALL NOT run. MCP Op SHALL NOT persist.

#### Scenario: CLI Op with --save persists
- **WHEN** a CLI transform Op with `--save` succeeds on Player
- **THEN** Document History gained exactly one transform Command
- **AND** the Scene Asset on disk includes that transform
