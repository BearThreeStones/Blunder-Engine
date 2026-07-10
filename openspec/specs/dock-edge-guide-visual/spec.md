# dock-edge-guide-visual Specification

## Purpose
TBD - created by archiving change host-edge-guide-flat-style. Update Purpose after archive.
## Requirements
### Requirement: Shared flat-wide edge guide pictograph

The docking UI SHALL provide shared Slint components `DockEdgeGuideIcon` and `DockEdgeGuideButton` that render a flat-wide perimeter guide icon: white inward arrow on the outer side, solid blue chrome bar, and full light-blue body fill without dashed split dividers.

#### Scenario: Top edge orientation

- **WHEN** `DockEdgeGuideButton` is rendered with edge top (2)
- **THEN** a white downward triangle SHALL appear above the icon body
- **AND** the body SHALL be landscape (wider than tall)
- **AND** the body SHALL NOT show a half-pane active region or dashed divider

#### Scenario: Left edge portrait orientation

- **WHEN** `DockEdgeGuideButton` is rendered with edge left (0)
- **THEN** a white left-pointing triangle SHALL appear on the outer (left) side
- **AND** the body SHALL be portrait (taller than wide)

### Requirement: Edge guide faint and highlight states

`DockEdgeGuideButton` SHALL support `faint` and `highlighted` properties. When `faint` is true and `highlighted` is false, the component SHALL render at reduced opacity. When `highlighted` is true, the frame stroke SHALL increase in width and use the highlight color; body opacity and arrow color SHALL remain unchanged.

#### Scenario: Faint host guides near perimeter

- **WHEN** `faint` is true and `highlighted` is false
- **THEN** the entire guide (arrow + body) SHALL render at approximately 45% opacity

#### Scenario: Highlight on hover

- **WHEN** `highlighted` is true
- **THEN** the icon frame border width SHALL be 2px and use the highlight stroke color
- **AND** the arrow fill and body opacity SHALL NOT change from the non-highlighted state

### Requirement: Shared metrics for edge guide dimensions

Edge guide dimensions SHALL be defined in a shared metrics global (`DockEdgeGuideMetrics` or consolidated from existing guide metrics) with at minimum: arrow size, arrow gap, landscape icon width/height, portrait icon width/height (swapped), chrome bar height, and body opacity.

#### Scenario: Top-bottom combined bounds include arrow

- **WHEN** a landscape icon is 48×28 with 8px arrow and 3px gap
- **THEN** the top-edge combined component bounds SHALL be 48×39 (arrow above body)

#### Scenario: Left-right combined bounds include arrow

- **WHEN** a portrait icon is 28×48 with 8px arrow and 3px gap
- **THEN** the left-edge combined component bounds SHALL be 39×48 (arrow beside body)

### Requirement: Auto-hide guide reuses shared visual primitives

`DockAutoHideGuideButton` SHALL compose `DockEdgeGuideIcon` and the shared arrow component rather than duplicating visual layout logic. Auto-hide-specific behavior and layer wiring SHALL remain in `DockAutoHideGuidesLayer`.

#### Scenario: Visual parity after refactor

- **WHEN** `DockAutoHideGuideButton` is rendered for any edge after the refactor
- **THEN** its appearance SHALL match the pre-refactor `DockAutoHideGuideButton` output for the same edge, highlight state, and metrics

