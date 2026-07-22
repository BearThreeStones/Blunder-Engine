## ADDED Requirements

### Requirement: User-level Project List persistence
The system SHALL persist a Project List outside any Project directory (user config location). The list SHALL record at least each Project's absolute root path and enough metadata to show a display name. Reloading Project Manager SHALL restore the persisted entries.

#### Scenario: List survives restart
- **WHEN** the user Imports or Creates a Project and later relaunches Project Manager
- **THEN** that Project still appears in the Project List

### Requirement: Register updates existing path
Adding a Project whose root path is already in the Project List SHALL update that entry (for example refresh display name / last-opened) rather than creating a duplicate path entry.

#### Scenario: Re-import same path
- **WHEN** the user Imports a Project path that is already listed
- **THEN** the list still contains a single entry for that path

### Requirement: Optional last-opened metadata
The Project List MAY store last-opened time per entry. When present, Project Open and successful Create/Import Open SHALL refresh that timestamp for the opened Project.

#### Scenario: Open updates last-opened
- **WHEN** the user successfully opens a listed Project
- **THEN** that entry's last-opened metadata is updated in the persisted Project List
