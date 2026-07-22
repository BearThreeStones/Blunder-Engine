# project-manager-chrome Specification

## Purpose

Godot-shaped Project Manager window layout and list presentation for MVP actions only (Create, Import, Open, Remove), without non-MVP Hub affordances.

## Requirements

### Requirement: Godot-shaped Project Manager chrome
The Project Manager window SHALL present a Godot-like layout: a top Projects header, a Create/Import action strip, a central Project List, a side column with Open and Remove, and a status footer. The UI language SHALL be English. Non-MVP Godot affordances (including Scan, favorites, filter/sort, Asset Library, Settings, Run, Rename, Duplicate, Tags, Remove Missing, and Donate) SHALL be omitted from the chrome rather than shown disabled.

#### Scenario: MVP actions remain reachable
- **WHEN** the user views the Project Manager with one or more listed Projects
- **THEN** Create, Import, Open, and Remove are available in the Godot-shaped layout and Project Open still uses the Open control

#### Scenario: Non-MVP Godot controls are absent
- **WHEN** the user views the Project Manager
- **THEN** Scan, favorites, Asset Library, Settings, and other non-MVP Godot side actions are not present in the UI

### Requirement: List row shows last opened time
Each Project List row SHALL show the project name, path, missing state, and last-opened time derived from the Project List entry. When last-opened is unset, the row SHALL show an empty or placeholder time rather than failing to render.

#### Scenario: Row includes last-opened metadata
- **WHEN** a Project List entry has a last-opened timestamp
- **THEN** the corresponding Manager list row displays that time alongside name and path

#### Scenario: Missing projects remain marked
- **WHEN** a listed Project is missing on disk
- **THEN** the row remains visibly marked missing and Open does not start an Editor Session

### Requirement: Create and Import dialogs match shell visuals
Create and Import SHALL remain modal overlays with existing confirm/cancel and browse behavior. Their visual styling SHALL align with the updated Project Manager shell (palette and spacing). Create/Import validation and Project Open-after-success behavior SHALL NOT change.

#### Scenario: Create dialog still creates and opens
- **WHEN** the user completes Create with a valid empty target
- **THEN** the Project is scaffolded, added to the Project List, and Project Open proceeds as before

#### Scenario: Import dialog still imports and opens
- **WHEN** the user completes Import with a valid Project path
- **THEN** the Project is registered and Project Open proceeds as before
