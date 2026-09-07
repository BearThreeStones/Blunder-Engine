# engine-gpu-cache Specification

## Purpose
Persist Engine shader GPU bytecode and the device pipeline-object cache outside any Project so a later Editor or Player start on this machine can skip a full source compile when that work is still valid.

## Requirements

### Requirement: Engine GPU cache is user-level and shared
The process SHALL store Shader bytecode cache and Pipeline cache in a user-level Engine GPU cache directory that is outside every Project, outside `.blunder/`, and outside Cooked cache. The Editor process and the Player process on that machine SHALL resolve the same default directory. The blobs SHALL NOT be Assets and SHALL NOT be Cook output.

#### Scenario: Second editor start reuses bytecode
- **WHEN** the author empties the Engine GPU cache, opens the editor once (viewport draws), then opens the same editor again on the same GPU without changing Engine shader sources
- **THEN** unchanged Engine shaders are not compiled from source
- **AND** the viewport still draws correctly

#### Scenario: Player uses the same directory
- **WHEN** the author enters Play from the editor (Player is a separate process)
- **THEN** that Player uses the same default Engine GPU cache directory as the Editor
- **AND** Play still draws correctly

### Requirement: Bytecode cache carries layout from the same compile
A Shader bytecode cache hit SHALL restore GPU bytecode together with the Shader resource layout produced by that compile. The process SHALL NOT recover layout by parsing SPIR-V. After a hit, the shared graphics-pipeline path SHALL still refuse to finish initialization unless that layout’s binding set equals the record path’s writes.

#### Scenario: Unchanged shader skips source compile
- **WHEN** an Engine shader’s source and compile identity still match a valid bytecode cache entry
- **THEN** process start uses the persisted bytecode and Shader resource layout
- **AND** it does not compile that shader from source

#### Scenario: Source change misses bytecode
- **WHEN** the author changes `pbr.slang` and starts the editor again
- **THEN** that shader’s bytecode cache entry is not used
- **AND** the shader is compiled from source
- **AND** the viewport still draws correctly

#### Scenario: Cached layout mismatch still fails start
- **WHEN** a bytecode cache hit restores a Shader resource layout whose binding set is not exactly the record path’s writes
- **THEN** process start fails before a viewport is presented
- **AND** the mismatch is not treated as a cache miss

### Requirement: Corrupt or stale cache rebuilds without failing start
A missing, unreadable, or integrity-failed Engine GPU cache blob SHALL be discarded and rebuilt. That condition SHALL NOT abort process start. A device Pipeline cache identity change SHALL miss Pipeline cache only; bytecode MAY still hit. An Engine/Slang compile-identity change SHALL miss both caches.

#### Scenario: Corrupt file does not FATAL
- **WHEN** an Engine GPU cache file is corrupted and the author starts the editor
- **THEN** the editor still starts
- **AND** the viewport still draws correctly
- **AND** that blob is discarded and rebuilt rather than aborting the process

#### Scenario: Device change misses Pipeline cache only
- **WHEN** the Pipeline cache device identity differs from the stored Pipeline cache blob and Engine shader sources are unchanged
- **THEN** Pipeline cache is not reused
- **AND** Shader bytecode cache may still be used
