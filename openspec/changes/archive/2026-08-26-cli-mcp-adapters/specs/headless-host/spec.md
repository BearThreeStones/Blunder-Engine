## ADDED Requirements

### Requirement: Adapter launch implies Headless
`--mcp` or a CLI verb SHALL start a Headless Editor Session even when `--headless` is omitted. Redundant `--headless` SHALL be accepted. A windowed adapter launch SHALL fail closed. Bare `--headless` without an adapter SHALL remain valid.

#### Scenario: CLI verb without --headless
- **WHEN** `engine_editor` is launched with `--project-root`, `--scene`, and CLI Capture, without `--headless`
- **THEN** the Editor Session is Headless
- **AND** Capture can run

### Requirement: Adapter requires explicit project root
CLI and MCP SHALL require `--project-root`. Debug compiled `BLUNDER_PROJECT_ROOT` SHALL NOT open a Project for those launches.

#### Scenario: Headless without adapter still uses Debug root
- **WHEN** a Debug `engine_editor` is launched with `--headless` and without `--project-root` and without `--mcp` or a CLI verb
- **THEN** the compiled Debug project root may still open (existing Editor entry)
