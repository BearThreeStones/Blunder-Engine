## Purpose

Named Editor Theme tokens (Base layers, accent, radii, depth, type size) so editor chrome has one visual source of truth instead of ad-hoc hex.

## ADDED Requirements

### Requirement: Named theme tokens are the color source of truth
The editor SHALL expose named Editor Theme variables for Base layers, hairline, text, icon, button, field, titlebar, and **Editor accent** (`#4C8DFF`) plus derived accent-soft, accent-line, and accent-hover tints. Product chrome SHALL read those variables. Light theme SHALL NOT be required.

#### Scenario: Accent is one token
- **WHEN** Play, a checked Viewport tool, a Hierarchy selected row, a Content Browser selected thumbnail, and a Project Manager selected project are all visible
- **THEN** each uses the same accent token (solid, soft fill, or line tint derived from it) rather than unrelated hardcoded blues

#### Scenario: No Light theme this pass
- **WHEN** the editor runs
- **THEN** it presents the dark Editor Theme and does not offer a Light theme switch

### Requirement: Three Base layers and recessed fields
Editor chrome SHALL stack three backgrounds, darkest first: Application Bar Base 1 (`#191B1F`), Window Base 2 (`#2A2D31`), Toolbar / raised Base 3 (`#34383D`). Recessed fields and Inspector property-field cells SHALL sit below Window (`#1E2125`). Window brightness SHALL stay near Godot editor panel `#292929` and SHALL NOT drop to near-black (`#1B1E22` or darker).

#### Scenario: Window is lighter than Application Bar
- **WHEN** an Editor Session shows the Application Bar above a docked Hierarchy
- **THEN** the Hierarchy panel surface is lighter than the Application Bar and darker than Toolbar / raised controls on that panel

#### Scenario: Fields recede under the panel
- **WHEN** an Inspector property field or search field is shown on a Window surface
- **THEN** the field fill is darker than that Window surface

### Requirement: Editor depth without bevels
Layering SHALL use 1px hairlines (`#3B3F45`) and optional soft shadows. Chrome SHALL NOT use inset/outset bevel (light-and-dark opposite edges) as the depth cue.

#### Scenario: Buttons have no bevel
- **WHEN** an Editor controls Button is shown in default state
- **THEN** its border is a hairline (or none, for ghost buttons) rather than a four-sided bevel

### Requirement: Editor corner radius
Controls SHALL use 6px radius. Docked window frames, Viewport tool strips, and Content Browser thumbnails SHALL use 10px. Editor modals SHALL use 14px. Hierarchy selection rectangles SHALL stay square.

#### Scenario: Modal is rounder than a Button
- **WHEN** an Editor modal and an Editor controls Button are both visible
- **THEN** the modal corner radius is larger than the Button radius

#### Scenario: Hierarchy selection stays square
- **WHEN** a Hierarchy row is selected
- **THEN** the selection fill is a square rectangle (not a rounded pill)
