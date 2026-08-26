## ADDED Requirements

### Requirement: Adapters emit stills as PNG
CLI Capture and CLI play-frame SHALL write PNG to `--out`. MCP Capture and MCP play-frame SHALL return PNG ImageContent. They SHALL NOT scrape HWND. They SHALL NOT return stills as Query or Diagnose.

#### Scenario: CLI Capture writes PNG
- **WHEN** CLI Capture succeeds with `--out`
- **THEN** `--out` is a PNG of the 16:9 Scene still
- **AND** stdout JSON does not contain the image bytes

### Requirement: CLI play-frame is the episode presentation
CLI play-frame SHALL produce one Play frame by composing the existing Play Session (start, optional Pause and Play step, one Play frame, Stop) in one process. MCP play-frame SHALL remain a Play frame request on an existing session. Both SHALL use the name play-frame. The CLI episode SHALL NOT be a second observation protocol and SHALL NOT be named capture-play.

#### Scenario: MCP play-frame needs an active session
- **WHEN** MCP play-frame is called while Play is Stopped
- **THEN** the result is Request failure
- **AND** no PNG ImageContent is returned
