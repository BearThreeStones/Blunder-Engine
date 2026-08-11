# detection-action Specification

## Purpose
Editor Detection Action for watched Source-archive and Intermediate-direct Asset changes: Prompt (default) or Auto Reimport, with path attribution including glTF sidecars and coalesced Prompt UI.

## Requirements

### Requirement: Detection Action preference
The editor SHALL expose a user-level Detection Action preference with at least two values: **Prompt** (product default) and **Auto**. Detection Action SHALL apply uniformly to Source-archive and Intermediate-direct watched changes attributed to Assets. Detection Action SHALL NOT replace manual Reimport.

#### Scenario: Default is Prompt
- **WHEN** a user has not overridden Detection Action
- **THEN** watched Source or Intermediate changes that map to Assets use Prompt behavior rather than silent Auto Reimport

#### Scenario: Auto runs Reimport without prompt
- **WHEN** Detection Action is Auto and a debounced watched change maps to one or more Assets
- **THEN** the editor runs Reimport for those Assets without requiring a confirmation toast

### Requirement: Path attribution including sidecars
Asset Watch SHALL map a changed file under the Project Assets or Resources trees to Asset GUID(s) using descriptor `source` and `archived_source`, and SHALL attribute sibling glTF `.bin` files and glTF-relative texture paths to the same Asset GUID set as their parent exchange glTF when those sidecars change.

#### Scenario: Intermediate glTF maps via source
- **WHEN** `resources/Animations/LOOP-idle/LOOP-idle.gltf` changes and a Clip descriptor `source` points at that path
- **THEN** Detection attributes that Clip GUID (and any other descriptors pointing at that path)

#### Scenario: Sidecar bin coalesces with parent
- **WHEN** `LOOP-idle.bin` beside `LOOP-idle.gltf` changes within the debounce window
- **THEN** Detection attributes the same Asset GUID set as for the parent glTF and does not require a separate user decision beyond the coalesced Detection event

### Requirement: Coalesced Prompt
When Detection Action is Prompt and one or more Assets are attributed in a debounce window, the editor SHALL show a single coalesced confirmation offering Reimport All and Dismiss (not one toast per Asset in v1).

#### Scenario: Multi-asset save one toast
- **WHEN** Mesh Intermediate and two companion glTF sidecars change in one debounce window and map to multiple GUIDs
- **THEN** the user sees one Prompt covering those Assets with Reimport All / Dismiss

### Requirement: Intermediate-direct change triggers Detection Reimport
A change to an Intermediate body under Resources (excluding treating Source-archive policy separately) that maps to an Asset SHALL enter Detection Action and, when confirmed or Auto, SHALL Reimport that Asset rather than only invalidating Finals.

#### Scenario: Overwrite Intermediate-direct mesh glTF
- **WHEN** Detection Action is Auto and an Intermediate-direct Mesh `source` glTF is overwritten on disk
- **THEN** Reimport runs for that Mesh GUID and Finals are invalidated as part of Reimport

#### Scenario: Prompt dismiss skips Reimport
- **WHEN** Detection Action is Prompt and the user chooses Dismiss
- **THEN** Reimport does not run for the prompted Assets from that toast
