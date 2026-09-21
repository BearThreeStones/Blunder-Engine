# Spec Delta

## ADDED Requirements

### Requirement: Cover is not the open-progress overlay
The Startup cover SHALL still dismiss when the Editor Shell is on screen. It SHALL NOT wait for Content Browser index refresh or startup scene instantiate. It SHALL NOT show a percentage. It SHALL NOT show elapsed seconds. Elapsed open time and discrete percent SHALL appear on Editor open progress after the Shell is on screen.

#### Scenario: Cover gone before overlay
- **WHEN** the Editor Shell is on screen and startup scene instantiate is still running
- **THEN** the Startup cover is gone
- **AND** the cover shows no percent and no elapsed seconds

#### Scenario: Cover still has no percent
- **WHEN** the Startup cover is visible during cook
- **THEN** no percent value is shown
- **AND** no elapsed-seconds readout is shown on the cover
