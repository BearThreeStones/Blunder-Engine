## MODIFIED Requirements

### Requirement: Console clear and Clear on Play
Console clear SHALL remove every Console Message (both origins) and SHALL NOT be an Editor Command. Clear on Play SHALL be a toggle, default on. When on, starting a Play session SHALL perform Console clear. Stop SHALL NOT clear. Play Reload SHALL NOT clear.

#### Scenario: Manual clear empties both origins
- **WHEN** the list has editor and Player rows and the author activates Clear
- **THEN** the list is empty

#### Scenario: Clear on Play at session start
- **WHEN** Clear on Play is on and the author starts Play after preflight succeeds
- **THEN** Console Messages from before that start are gone

#### Scenario: Stop does not clear
- **WHEN** Player rows exist and the author Stops Play
- **THEN** those rows remain

#### Scenario: Reload does not clear
- **WHEN** Clear on Play is on, Player rows exist, and Play Reload succeeds
- **THEN** those rows remain
