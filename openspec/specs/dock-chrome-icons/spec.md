# dock-chrome-icons Specification

## Purpose

Font-independent vector glyphs for dock chrome controls (close, pin, minimize) in the Slint editor UI. Replaces Unicode text glyphs that fail to render when the bundled UI font lacks those codepoints.

## Requirements

### Requirement: Vector close icon component

The editor SHALL provide a reusable Slint component that draws a close (×) icon using vector geometry (not text glyphs). The icon SHALL be legible at 9–12 px effective size on dark dock backgrounds and SHALL accept an `icon-color` property for default and hover styling by parent `TouchArea`.

#### Scenario: Close icon visible on dock tab

- **WHEN** a dock tab with a close button is displayed in the editor
- **THEN** the close control SHALL show a recognizable × shape
- **AND** it SHALL NOT render as a missing-glyph placeholder rectangle

#### Scenario: Close icon hover feedback

- **WHEN** the user hovers the dock tab close control
- **THEN** the × icon color SHALL brighten to match existing dock chrome hover behavior

### Requirement: Vector pin icon component

The editor SHALL provide a reusable Slint component that draws a pin icon using vector geometry (not emoji or text glyphs). The icon SHALL be legible at ~11 px on dock tab chrome.

#### Scenario: Pin icon visible on pinnable tab

- **WHEN** a dock tab with `show-pin` is displayed
- **THEN** the pin control SHALL show a recognizable pin shape
- **AND** it SHALL NOT render as a missing-glyph placeholder rectangle

### Requirement: Vector minimize icon component

The editor SHALL provide a reusable Slint component that draws a minimize (horizontal dash) icon using vector geometry. The icon SHALL be legible in auto-hide overlay title bars.

#### Scenario: Minimize icon visible on overlay title bar

- **WHEN** an expanded auto-hide overlay with `show-minimize` is displayed
- **THEN** the minimize control SHALL show a horizontal dash
- **AND** it SHALL NOT render as a missing-glyph placeholder rectangle

### Requirement: Dock chrome uses vector icons consistently

All dock chrome close, pin, and minimize affordances in `docking_panel.slint` SHALL use the vector icon components instead of Unicode `Text` glyphs. Affected surfaces: main dock tabs, auto-hide strip unpin, and auto-hide overlay title bar.

#### Scenario: Tab close uses vector icon

- **WHEN** the user views any docked panel tab (e.g. Viewport, Hierarchy)
- **THEN** the tab close button SHALL use the vector close component

#### Scenario: Auto-hide unpin uses vector close icon

- **WHEN** a widget is pinned to an auto-hide edge strip
- **THEN** the strip tab unpin control SHALL use the vector close component

#### Scenario: Overlay chrome uses vector icons

- **WHEN** an auto-hide overlay is expanded
- **THEN** its title bar minimize and close controls SHALL use the vector minimize and close components respectively

### Requirement: No behavior change to dock interactions

Replacing text glyphs with vector icons SHALL NOT change hit targets, callbacks, or docking/auto-hide logic. Close, pin, minimize, and unpin actions SHALL invoke the same callbacks as before.

#### Scenario: Tab close still closes widget

- **WHEN** the user clicks the vector close icon on a closable dock tab
- **THEN** `widget-closed` SHALL fire for that widget id as before
