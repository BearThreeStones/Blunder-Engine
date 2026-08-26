## Purpose

Editor Console: a docked diagnostic message list for the author, with severity filters, collapse, clear, and a bounded ring — not a command prompt, History Panel, or OS terminal.

## ADDED Requirements

### Requirement: Console dock sibling to Content Browser
The editor SHALL provide a Console panel as a sibling tab to Content Browser in the same bottom dock group. It SHALL NOT be an inner tab of History. The default workspace SHALL include this Console tab (authors MAY later undock it).

#### Scenario: Console tab beside Content Browser
- **WHEN** the editor session opens with the default dock layout
- **THEN** the bottom dock group includes Content Browser and Console as sibling tabs

#### Scenario: Console is not inside History
- **WHEN** the author opens the Content Browser panel's inner File System / History tabs
- **THEN** Console is not one of those inner tabs

### Requirement: Console Message list
The Console SHALL list Console Messages from the Editor Session and from the Play Process in one list. Each message SHALL have Console severity (Log, Warning, or Error), text, Console origin, and Console time (`HH:mm:ss` local). The list SHALL be oldest-at-top, newest-at-bottom. Selecting a row SHALL show Console detail (full text and Console stack; stack empty for engine `LOG_*` rows).

#### Scenario: Editor and Player rows share one list
- **WHEN** the editor logs an info message and the Play Process forwards a warning
- **THEN** both appear in the same Console list with distinct origins

#### Scenario: Oldest at top
- **WHEN** two Console Messages are recorded in order A then B
- **THEN** A is above B

### Requirement: Console severity map
`LOG_INFO` SHALL become Log. `LOG_WARN` SHALL become Warning. `LOG_ERROR` and `LOG_FATAL` SHALL become Error. `LOG_DEBUG` SHALL NOT create a Console Message. `LOG_FATAL` SHALL still abort via the existing fatal path after the Error row is recorded.

#### Scenario: Debug stays off the Console
- **WHEN** the engine logs at debug level
- **THEN** the Console ring does not gain a row
- **AND** an attached terminal MAY still receive the debug line

#### Scenario: Info is Log
- **WHEN** the engine logs at info level
- **THEN** the Console records a Log message with that text

### Requirement: Console capacity
The Console SHALL keep at most 10000 Console Messages (pre-collapse emits). Pushing past the limit SHALL drop the oldest message.

#### Scenario: Oldest dropped at 10000
- **WHEN** 10001 info messages are recorded with collapse off
- **THEN** the first message is gone and the ring holds 10000 messages

### Requirement: Console collapse
The Console SHALL offer a Collapse toggle, default off. When on, messages that share a Collapse key (text + severity + Console stack + origin) SHALL appear as one row with a count. A collapsed row SHALL show the Console time of the latest emit. Editor Session and Play Process messages SHALL NOT collapse together.

#### Scenario: Collapse off lists every emit
- **WHEN** Collapse is off and the same Log text is emitted three times from the editor
- **THEN** the list shows three rows

#### Scenario: Collapse on merges same key
- **WHEN** Collapse is on and the same Log text with the same empty stack is emitted three times from the editor
- **THEN** the list shows one row with count 3

#### Scenario: Origins do not merge
- **WHEN** Collapse is on and the editor and the Player emit the same Log text
- **THEN** the list shows two rows

### Requirement: Console clear and Clear on Play
Console clear SHALL remove every Console Message (both origins) and SHALL NOT be an Editor Command. Clear on Play SHALL be a toggle, default on. When on, starting a Play session SHALL perform Console clear. Stop SHALL NOT clear.

#### Scenario: Manual clear empties both origins
- **WHEN** the list has editor and Player rows and the author activates Clear
- **THEN** the list is empty

#### Scenario: Clear on Play at session start
- **WHEN** Clear on Play is on and the author starts Play after preflight succeeds
- **THEN** Console Messages from before that start are gone

#### Scenario: Stop does not clear
- **WHEN** Player rows exist and the author Stops Play
- **THEN** those rows remain

### Requirement: Error Pause
Error Pause SHALL be a Console toggle, default off. When on, an Error Console Message with Play Process origin SHALL issue Play Pause. Editor Session origin Errors SHALL NOT pause the Player. Warnings SHALL NOT pause.

#### Scenario: Player Error pauses when enabled
- **WHEN** Error Pause is on, Play is Playing, and a Play Process Error is recorded
- **THEN** the editor issues Play Pause

#### Scenario: Editor Error does not pause
- **WHEN** Error Pause is on, Play is Playing, and an Editor Session Error is recorded
- **THEN** the Player is not paused for that Error

### Requirement: Console filter
The Console SHALL provide Log, Warning, and Error toggles (default all on) and a case-insensitive text search over message text. A row SHALL show when its severity toggle is on and the search is empty or matches. Toggle counts SHALL be pre-collapse ring counts per severity.

#### Scenario: Error-only filter
- **WHEN** Log and Warning toggles are off, Error is on, and the ring has mixed severities
- **THEN** only Error rows are visible

#### Scenario: Search matches text
- **WHEN** the search is `clip` and a Log row text contains `clip missing`
- **THEN** that row is visible if the Log toggle is on

### Requirement: Product path does not AllocConsole
Editor Session and Player SHALL NOT allocate a new OS console window as the product path. Log/Warning/Error SHALL still appear in the editor Console without an attached terminal. Debug MAY be invisible when no attached terminal exists.

#### Scenario: Editor starts without a system console window
- **WHEN** the author launches `engine_editor` as a Windows GUI subsystem process with no inherited console
- **THEN** the process does not create a new OS console window
- **AND** Log-severity engine messages still appear in the Console panel
