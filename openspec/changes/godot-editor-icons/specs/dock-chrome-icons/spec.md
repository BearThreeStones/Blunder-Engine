## MODIFIED Requirements

### Requirement: Vector close icon component

The editor SHALL provide a reusable Slint component that shows a close (×) icon using the vendored Godot `GuiClose` SVG (not text glyphs). The icon SHALL be legible at 9–12 px effective size on dark dock backgrounds and SHALL accept an `icon-color` property applied via Slint `Image.colorize` for default and hover styling by parent `TouchArea`.

#### Scenario: Close icon visible on dock tab

- **WHEN** a dock tab with a close button is displayed in the editor
- **THEN** the close control SHALL show a recognizable × shape from the Godot `GuiClose` Editor Icon
- **AND** it SHALL NOT render as a missing-glyph placeholder rectangle

#### Scenario: Close icon hover feedback

- **WHEN** the user hovers the dock tab close control
- **THEN** the × icon color SHALL brighten to match existing dock chrome hover behavior

### Requirement: Vector pin icon component

The editor SHALL provide a reusable Slint component that shows a pin icon using the vendored Godot `Pin` SVG (not emoji or text glyphs). The icon SHALL be legible at ~11 px on dock tab chrome and SHALL accept `icon-color` via `Image.colorize`.

#### Scenario: Pin icon visible on pinnable tab

- **WHEN** a dock tab with `show-pin` is displayed
- **THEN** the pin control SHALL show a recognizable pin shape from the Godot `Pin` Editor Icon
- **AND** it SHALL NOT render as a missing-glyph placeholder rectangle

### Requirement: Vector minimize icon component

The editor SHALL provide a reusable Slint component that draws a minimize (horizontal dash) icon using vector geometry (not text glyphs and not a Godot SVG, unless a dedicated minus glyph is later adopted). The icon SHALL be legible in auto-hide overlay title bars and SHALL accept an `icon-color` property for styling by parent `TouchArea`.

#### Scenario: Minimize icon visible on overlay title bar

- **WHEN** an expanded auto-hide overlay with `show-minimize` is displayed
- **THEN** the minimize control SHALL show a horizontal dash
- **AND** it SHALL NOT render as a missing-glyph placeholder rectangle

### Requirement: Dock chrome uses vector icons consistently

All dock chrome close, pin, and minimize affordances in `docking_panel.slint` SHALL use the dock chrome icon components instead of Unicode `Text` glyphs. Close and pin SHALL use Godot Editor Icons as specified above; minimize SHALL use the geometry component. Affected surfaces: main dock tabs, auto-hide strip unpin, and auto-hide overlay title bar.

#### Scenario: Tab close uses vector icon

- **WHEN** the user views any docked panel tab (e.g. Viewport, Hierarchy)
- **THEN** the tab close button SHALL use the dock chrome close component backed by Godot `GuiClose`

#### Scenario: Auto-hide unpin uses vector close icon

- **WHEN** a widget is pinned to an auto-hide edge strip
- **THEN** the strip tab unpin control SHALL use the dock chrome close component

#### Scenario: Overlay chrome uses vector icons

- **WHEN** an auto-hide overlay is expanded
- **THEN** its title bar minimize and close controls SHALL use the minimize and close dock chrome components respectively
