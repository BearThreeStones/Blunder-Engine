## ADDED Requirements

### Requirement: C-ABI Console log entry
The engine C-ABI SHALL expose a log entry (`blunder_log` or NativeAbi-equivalent) that accepts a Console severity, UTF-8 message text, and optional UTF-8 Console stack. A successful call SHALL record a Console Message in the process Console ring (Player process: also eligible for Play log forwarding). `debug` severity SHALL NOT record a Console Message.

#### Scenario: ABI version is at least 11
- **WHEN** `blunder_engine_abi_version` is queried after this change
- **THEN** the returned value is >= 11

#### Scenario: Native log without managed host
- **WHEN** a native caller invokes the log entry with Warning severity and text `clip missing` and a null stack
- **THEN** the process Console ring contains a Warning whose text is `clip missing`
