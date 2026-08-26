## Purpose

CLI and MCP are Headless Editor Session presentations of one verb set so agents and CI can call Authorship, Host observation, and Play Session without a windowed GUI, a third process, or a second catalog.

## ADDED Requirements

### Requirement: One verb set, two presentations
CLI and MCP SHALL present the same verb set. MCP SHALL expose Query, Op, Diagnose, Capture, Play, Pause, Resume, Stop, Play step, play-frame, and Save. CLI SHALL expose Query, Op, Diagnose, Capture, and play-frame. CLI SHALL NOT expose granular Play / Pause / Resume / Stop / Play step or a standalone Save verb. The presentations SHALL NOT grow a second machine catalog.

#### Scenario: MCP lists the granular Play verbs
- **WHEN** an MCP client lists tools
- **THEN** the list includes play, pause, resume, stop, step, and play-frame as distinct tools

#### Scenario: CLI has no standalone save
- **WHEN** CLI is invoked with a `save` verb and no Op
- **THEN** the invocation is a Request failure
- **AND** the process exits non-zero

### Requirement: Adapters live in engine_editor
CLI and MCP SHALL run in the `engine_editor` process. The Player SHALL NOT host CLI or MCP. The product SHALL NOT add `engine_agent` or listen on a TCP/HTTP port for v1 MCP.

#### Scenario: Player has no MCP
- **WHEN** `engine_player` is launched
- **THEN** it does not accept `--mcp` as an adapter server

### Requirement: Adapter session is Headless and unattached
`--mcp` or a CLI verb SHALL imply a Headless Editor Session. A windowed adapter launch SHALL fail closed. The adapter Session SHALL NOT attach to another Editor Session on the same project root. Bare `--headless` without `--mcp` or a CLI verb SHALL remain a valid Headless Editor.

#### Scenario: MCP implies Headless
- **WHEN** `engine_editor` is launched with `--mcp` and `--project-root` and without `--headless`
- **THEN** the Editor Session is Headless
- **AND** no editor OS window is created

#### Scenario: Windowed MCP fails
- **WHEN** an adapter launch is requested as a windowed Editor Session
- **THEN** the result is Request failure
- **AND** no adapter server starts

### Requirement: Adapter requires project root
CLI and MCP launches SHALL require `--project-root`. They SHALL NOT fall back to Debug `BLUNDER_PROJECT_ROOT` when `--project-root` is omitted.

#### Scenario: Debug MCP without project-root fails
- **WHEN** a Debug `engine_editor` is launched with `--mcp` and without `--project-root`
- **THEN** the result is Request failure `launch.project_root_required`
- **AND** the compiled Debug project root is not opened

### Requirement: Live requires --scene
A CLI or MCP session SHALL have a Live document only when launched with `--scene` naming that scene's virtual path. Without `--scene`, Live Query, Live Diagnose, Op, Live Capture, and MCP Save SHALL fail as Request failure `subject.no_live_document`. On-disk Query, Diagnose, and Capture SHALL still name a Scene Asset virtual path. Play Session verbs and CLI play-frame SHALL require `--scene` as the Play entry; without it they SHALL fail as Request failure `launch.scene_required`.

#### Scenario: Live Capture without --scene
- **WHEN** CLI Capture is requested with subject Live and the launch omitted `--scene`
- **THEN** the result is Request failure `subject.no_live_document`
- **AND** no PNG is written

#### Scenario: On-disk Diagnose without Live
- **WHEN** CLI Diagnose is requested with subject On-disk and a Scene Asset virtual path, and the launch omitted `--scene`
- **THEN** Diagnose runs against that saved Scene Asset

### Requirement: MCP is stdio
The MCP adapter SHALL speak MCP JSON-RPC on stdin/stdout of that Editor Session. It SHALL NOT listen on a port.

#### Scenario: No listen port
- **WHEN** MCP is running
- **THEN** the process does not bind a TCP or HTTP listen port for the adapter

### Requirement: CLI is one verb then exit
The CLI adapter SHALL accept exactly one verb per process, run it, print one JSON object to stdout, and exit. It SHALL NOT offer a long-lived CLI REPL. stderr SHALL carry Host logs, not the machine result.

#### Scenario: Capture JSON and PNG
- **WHEN** CLI Capture succeeds with `--out` set
- **THEN** stdout is one JSON object with ok true
- **AND** the PNG is written to `--out`
- **AND** the process exits 0

### Requirement: CLI Request failure vs Diagnose Issues
A CLI Request failure SHALL exit non-zero and put `failure_code` in the stdout JSON. A Diagnose that ran SHALL exit 0 even when the Issue list contains Error Issues.

#### Scenario: Diagnose Error Issues are not a request failure
- **WHEN** CLI Diagnose runs and returns an Error Issue `play.missing_camera`
- **THEN** the process exits 0
- **AND** the stdout JSON contains that Issue

#### Scenario: Missing --out is request failure
- **WHEN** CLI Capture or CLI play-frame is invoked without `--out`
- **THEN** the result is Request failure `cli.out_required`
- **AND** the process exits non-zero

### Requirement: CLI Op requires --save
A CLI Op without `--save` SHALL fail as Request failure `cli.save_required` and SHALL NOT push Editor History. A CLI Op with `--save` SHALL commit the Op then persist the Live document through the editor save path. MCP Op SHALL NOT persist. MCP Save SHALL persist the Live document and SHALL NOT be an Op and SHALL NOT push Editor History.

#### Scenario: CLI Op without --save
- **WHEN** CLI Op is invoked without `--save`
- **THEN** the result is Request failure `cli.save_required`
- **AND** Document History is unchanged
- **AND** the Scene Asset on disk is unchanged

#### Scenario: MCP Save is not History
- **WHEN** MCP Save succeeds on a dirty Live document
- **THEN** the Scene Asset on disk matches the Live document
- **AND** Document History does not gain a Save Command

### Requirement: Still encoding
CLI Capture and CLI play-frame SHALL write a PNG file to `--out` (overwrite if the file exists). MCP Capture and MCP play-frame SHALL return PNG ImageContent. CLI stdout SHALL NOT be a raw PNG stream. MCP SHALL NOT return a filesystem path as the still.

#### Scenario: MCP Capture is ImageContent
- **WHEN** MCP Capture succeeds
- **THEN** the tool result includes PNG ImageContent
- **AND** the result is not a required filesystem path

### Requirement: CLI play-frame episode
CLI play-frame SHALL start Play on the Headless Play Session, then take one Play frame, then Stop, then exit. When `--steps` is omitted or 0, that frame SHALL be taken while Playing after the Player is ready. When `--steps` is N greater than 0, the session SHALL Pause, Play step N ticks, take the Play frame, then Stop. MCP play-frame SHALL only request a Play frame on an already running Play Session.

#### Scenario: Default steps is Playing frame
- **WHEN** CLI play-frame runs with `--scene` and `--out` and without `--steps`
- **THEN** a Headless Player is spawned
- **AND** one Play frame PNG is written after ready while Playing
- **AND** the Player is Stopped before the editor process exits

#### Scenario: Steps require Pause
- **WHEN** CLI play-frame runs with `--steps 10`
- **THEN** Play step runs only after Pause
- **AND** one Play frame PNG is written
- **AND** the Player is Stopped before exit
