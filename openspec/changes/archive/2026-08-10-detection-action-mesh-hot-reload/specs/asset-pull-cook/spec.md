## MODIFIED Requirements

### Requirement: Asset Watch invalidation
The editor SHALL watch the Assets root and Resources root (Source archive and Intermediate bodies). Descriptor and other Assets-tree changes SHALL invalidate Finals for affected Assets and dependents via the dependency graph. Intermediate body changes that map to Assets SHALL be handled primarily by Detection Action → Reimport (see Detection Action specs); if Reimport does not run (Prompt dismissed or attribution miss), the editor SHALL still invalidate Finals for mapped Assets when a watched Intermediate or descriptor change is observed.

#### Scenario: Descriptor change invalidates Final
- **WHEN** a mesh or texture Asset Descriptor file changes on disk
- **THEN** that Asset’s Final is marked stale

#### Scenario: Intermediate change without Reimport still invalidates
- **WHEN** an Intermediate body mapped to an Asset changes and Detection Prompt is dismissed
- **THEN** that Asset’s Final is marked stale even though Reimport did not run

### Requirement: Source change triggers Reimport
Changes under the Source root for an archived Source file SHALL enter Detection Action for Assets that archive that Source (debounced, including sidecar attribution rules where applicable). Reimport SHALL run only when Detection Action is Auto or the user confirms Prompt — not as an unconditional silent auto-Reimport.

#### Scenario: Archived Source change with Auto Detection
- **WHEN** Detection Action is Auto and an archived Source file watched under the Source root changes
- **THEN** Reimport runs for the owning Asset GUID(s), refreshing Intermediate and invalidating Finals

#### Scenario: Archived Source change with Prompt Detection
- **WHEN** Detection Action is Prompt and an archived Source file changes
- **THEN** the editor prompts before Reimport and does not silently Reimport without confirmation
