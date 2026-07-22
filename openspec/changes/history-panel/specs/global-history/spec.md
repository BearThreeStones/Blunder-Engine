## ADDED Requirements

### Requirement: Global History is a separate empty stack
The editor SHALL maintain a Global History stack independent from Document History for non-document editor actions. In this milestone Global History SHALL accept the same command-stack operations as Document History but SHALL ship with no Global Commands pushed. Opening a scene SHALL NOT merge Global History into Document History.

#### Scenario: Global starts empty
- **WHEN** the editor starts a session
- **THEN** Global History has no commands and cannot undo

#### Scenario: Independence from Document History
- **WHEN** the user pushes scene Commands onto Document History
- **THEN** Global History command count remains unchanged

### Requirement: Global History is panel-filterable
Global History SHALL be the backing store for the History Panel Global scope filter. Until Global Commands exist, filtering to Global alone SHALL yield an empty list.

#### Scenario: Filter Global alone
- **WHEN** the History Panel shows only the Global scope and Global History is empty
- **THEN** no history rows are listed
