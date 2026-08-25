## ADDED Requirements

### Requirement: Add… and Remove use Editor controls chrome
The Add… picker button, Add… popup chrome, and Unique-attachment Remove controls SHALL use Editor controls (Foldout / Button look) and Editor Theme colors. Grouping, uniqueness, cascade, single-select gate, and which types appear SHALL stay as specified by the other Add… requirements. Add… SHALL NOT be restyled into a Unity-style component menu information architecture.

#### Scenario: Add… is an Editor controls affordance
- **WHEN** exactly one entity is selected
- **THEN** Add… is visible as an Editor controls Button or equivalent
- **AND** opening it still lists Unique attachments, Behaviours, and Skeleton Modifiers in that grouped order

#### Scenario: Remove stays on the section header
- **WHEN** a Camera section is present
- **THEN** Remove remains on that section header and uses Editor controls chrome
