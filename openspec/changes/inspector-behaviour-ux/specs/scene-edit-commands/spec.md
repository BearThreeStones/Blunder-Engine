## ADDED Requirements

### Requirement: Behaviour declaration edits are Editor Commands
Successfully adding a Behaviour, removing a Behaviour, reordering the Behaviour list, or committing a Behaviour property field (Enter or focus loss) SHALL push an Editor Command on Document History for the active scene. Commands SHALL target EntityId. Intermediate typed property drafts SHALL NOT each push a Command.

#### Scenario: Undo Remove Behaviour
- **WHEN** the author Removes a Behaviour and undoes
- **THEN** the Behaviour slot (type, BehaviourId, property bag) is restored on that entity's Object in its prior list position

#### Scenario: Property scrub without commit pushes nothing
- **WHEN** the author changes a Behaviour property draft without Enter or focus loss
- **THEN** Document History does not gain a new Command for that draft
