## ADDED Requirements

### Requirement: Responsive Hierarchy population on scene activation
Activating a scene SHALL populate the visible Hierarchy without a quadratic pause as entity count grows. In the first-party Windows Debug benchmark, a flat scene containing 10,000 visible entities SHALL produce its Hierarchy rows within 500 milliseconds after scene data is already in memory.

#### Scenario: Large flat scene activation
- **WHEN** an in-memory scene with 10,000 visible root entities becomes active
- **THEN** all 10,000 Hierarchy rows are available within 500 milliseconds in the Windows Debug benchmark

#### Scenario: Large nested scene activation
- **WHEN** an in-memory scene contains nested entities with expanded and collapsed branches
- **THEN** the Hierarchy preserves entity order, depth, expansion, last-sibling guides, icons, and tombstone filtering while populating rows
