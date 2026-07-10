## MODIFIED Requirements

### Requirement: Same-pixel click cycling

Repeated left-clicks on the same viewport pixel SHALL cycle through the peeled hit list when more than one pickable entity exists at that pixel. The peeled list SHALL be produced by the same depth-peel path as the piercing menu and SHALL respect zero-to-one (`perspectiveZO`) depth ordering.

#### Scenario: Cycle forward on repeat click

- **WHEN** the user left-clicks the same viewport pixel twice without moving more than a small tolerance (default 3 px) and multiple entities are pickable at that pixel
- **THEN** the second click SHALL select the next entity in front-to-back order after the currently selected entity in that hit list (wrapping to front)

#### Scenario: Multi-peel populates cycle list on second click

- **WHEN** the first left-click at a pixel returns a single front-most hit and a second pickable surface exists behind it along the ray
- **THEN** the second left-click at the same pixel (within tolerance, no modifiers) SHALL issue a multi-peel pick that yields at least two promoted hits for cycling

#### Scenario: Single hit unchanged

- **WHEN** the user repeats left-click on a pixel with only one pickable entity along the ray
- **THEN** behavior SHALL match a normal replace selection (no spurious cycling)
