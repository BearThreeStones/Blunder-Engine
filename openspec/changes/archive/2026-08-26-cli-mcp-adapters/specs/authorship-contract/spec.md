## ADDED Requirements

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
