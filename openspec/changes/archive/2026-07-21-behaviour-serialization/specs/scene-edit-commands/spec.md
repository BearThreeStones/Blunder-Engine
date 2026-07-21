## ADDED Requirements

### Requirement: Soft-deleted entities omit Behaviours on save
Save and export SHALL omit soft-deleted entities entirely, including any Behaviour declarations they had while tombstoned in the live session.

#### Scenario: Tombstone drops behaviours from disk
- **WHEN** a user soft-deletes an entity that had Behaviours and saves
- **THEN** the written scene asset contains neither that entity nor its Behaviours
