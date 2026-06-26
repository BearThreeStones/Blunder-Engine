## MODIFIED Requirements

### Requirement: Host guide icon-only hit target

Host edge dock drop detection SHALL use the full host guide component rectangle (arrow plus icon body). Full-edge bands SHALL NOT activate host edge dock.

#### Scenario: Pointer on icon activates host dock target

- **WHEN** the pointer is inside the right host guide component bounds (including arrow and body)
- **THEN** the drag controller SHALL set the hovered host dock slot to right
- **AND** SHALL NOT require the pointer to be in a full-height band along the host right edge

#### Scenario: Pointer in edge zone without icon does not host-dock

- **WHEN** the pointer is within the auto-hide edge zone on the right but not over the host guide component bounds
- **THEN** the system SHALL NOT treat the hover as a host edge dock target
- **AND** MAY treat it as an auto-hide edge target if pinnable

### Requirement: Host guide visual style

Host edge guide icons SHALL use the shared flat-wide `DockEdgeGuideButton` pictograph (white inward arrow, blue chrome bar, full light-blue body). They SHALL NOT use the square `DockGuideButton` half-pane style with dashed dividers used by container split cross guides.

#### Scenario: Host guide uses flat-wide arrow icon

- **WHEN** host edge guide icons are visible during drag
- **THEN** they SHALL render via `DockEdgeGuideButton` with orientation per host edge
- **AND** they SHALL include a white inward arrow on the outer side of the host rect

#### Scenario: Host guide distinct from container cross

- **WHEN** host edge guide icons and container cross guides are visible during the same drag
- **THEN** host guides SHALL use the flat-wide arrow style
- **AND** container cross guides SHALL continue to use `DockGuideButton` half-pane style with dashed dividers
