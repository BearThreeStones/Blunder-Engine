## ADDED Requirements

### Requirement: Headless hosts omit profiler chrome and Tracy listen
A Headless Editor Session, Headless Player, and `--mcp` launch SHALL NOT mount the Frame timing HUD, SHALL NOT mount the Profiler dock, and SHALL NOT listen for a Tracy connection. Linux Merge CI SHALL NOT define `TRACY_ENABLE`. Missing or untrustworthy GPU timestamps SHALL NOT fail those hosts.

#### Scenario: MCP has no HUD
- **WHEN** `engine_editor` starts with `--mcp`
- **THEN** no Frame timing HUD SHALL be shown
- **AND** no Profiler dock SHALL be shown
- **AND** the process SHALL NOT listen for Tracy

#### Scenario: Merge CI compiles without Tracy
- **WHEN** Linux Merge CI configures the engine
- **THEN** `TRACY_ENABLE` SHALL NOT be defined
- **AND** configure and tests SHALL still run
